<?php

module_load_include('inc', 'modularform', 'modularform.rules');

function modularform_response_list_page() {
  global $user;

  // ── Base query ──────────────────────────────────────────────────────────────
  $form_id_filter = isset($_GET['form_id']) ? (int) $_GET['form_id'] : NULL;
  $pair_filters = isset($_GET['filters']) && is_array($_GET['filters'])
    ? array_values($_GET['filters'])
    : [];

  $query = db_select('modularform_responses', 'r')
    ->fields('r', ['id', 'form_id', 'email', 'submitted_at', 'updated_at'])
    ->orderBy('r.submitted_at', 'DESC');

  if ($form_id_filter) {
    $query->condition('r.form_id', $form_id_filter);
  }

  // ── Role-based visibility ───────────────────────────────────────────────────
  if (!_modularform_is_site_admin()) {
    $candidate_ids = $form_id_filter
      ? [$form_id_filter]
      : db_select('modularform_form_members', 'fm')
        ->fields('fm', ['form_id'])
        ->condition('fm.uid', $user->uid)
        ->execute()
        ->fetchCol();
    $all_access = [];
    $own_only = [];

    foreach ($candidate_ids as $fid) {
      $role = _modularform_form_member_role($fid);
      if (in_array($role, ['creator', 0, 1], TRUE)) {
        $all_access[] = (int) $fid;
      }
      elseif ($role === 2) {
        $own_only[] = (int) $fid;
      }
    }

    if (empty($all_access) && empty($own_only)) {
      return '<p>' . t('You do not have permission to view any responses.') . '</p>';
    }

    $or = db_or();
    if (!empty($all_access)) {
      $or->condition('r.form_id', $all_access, 'IN');
    }
    if (!empty($own_only)) {
      $or->condition(
        db_and()
          ->condition('r.form_id', $own_only, 'IN')
          ->condition('r.user_id', $user->uid)
      );
    }
    $query->condition($or);
  }

  // ── Solr filters ────────────────────────────────────────────────────────────
  $email_filter = isset($_GET['email']) ? check_plain(trim($_GET['email'])) : '';

  if (!empty($pair_filters) || $email_filter) {
    $fq = ['bundle:modularform_response'];

    if ($form_id_filter) {
      $fq[] = 'is_form_id:' . $form_id_filter;
    }

    foreach ($pair_filters as $pair) {
      if (empty($pair['solr_key'])) {
        continue;
      }
      if (!preg_match('/^[a-z]+_val_q_\d+_[a-z0-9]+$/', $pair['solr_key'])) {
        continue;
      }

      $solr_key = $pair['solr_key'];
      $has_date = !empty($pair['date_from']) || !empty($pair['date_to']);

      if ($has_date) {
        $from = !empty($pair['date_from']) ? _modularform_solr_date($pair['date_from'], 'start') : '*';
        $to = !empty($pair['date_to']) ? _modularform_solr_date($pair['date_to'], 'end') : '*';
        $fq[] = $solr_key . ':[' . $from . ' TO ' . $to . ']';
      }
      elseif (isset($pair['answer']) && $pair['answer'] !== '') {
        $fq[] = $solr_key . ':"' . addslashes($pair['answer']) . '"';
      }
    }

    if ($email_filter) {
      $fq[] = 'ss_email:"' . addslashes($email_filter) . '"';
    }

    module_load_include('inc', 'modularform_search', 'includes/modularform_forms_search');
    $docs = _modularform_search_solr_query('*:*', [
      'fq' => $fq,
      'fl' => 'entity_id',
      'rows' => 500,
    ]);

    if ($docs === NULL) {
      drupal_set_message(t('Filter service temporarily unavailable. Showing unfiltered results.'), 'warning');
    }
    elseif (empty($docs)) {
      $query->condition('r.id', [0], 'IN');
    }
    else {
      $ids = array_map(fn($doc) => (int) $doc->entity_id, $docs);
      $query->condition('r.id', $ids, 'IN');
    }
  }

  // ── Form metadata ───────────────────────────────────────────────────────────
  if ($form_id_filter) {
    $loaded = entity_get_controller('modularform_form')->load([$form_id_filter]);
    $form = reset($loaded);
    $forms = $form ? [$form->id => $form] : [];
  }
  else {
    $visible_ids = _modularform_is_site_admin()
      ? NULL
      : array_merge($all_access, $own_only);

    $q = db_select('modularform_forms', 'f')
      ->fields('f', ['id', 'title', 'editable', 'anonymous_responses']);

    if ($visible_ids !== NULL) {
      $q->condition('f.id', $visible_ids, 'IN');
    }

    $forms = $q->execute()->fetchAllAssoc('id');
  }

  $any_editable = FALSE;
  foreach ($forms as $f) {
    if ($f->editable) {
      $any_editable = TRUE;
      break;
    }
  }
  // ── Pager + fetch ───────────────────────────────────────────────────────────
  $responses = $query->extend('PagerDefault')->limit(25)->execute()->fetchAll();

  // ── Header ──────────────────────────────────────────────────────────────────
  $header = [t('Respondent'), t('Form'), t('Submitted')];
  if ($any_editable) {
    $header[] = t('Updated');
  }
  $header[] = t('Operations');

  // ── Rows ────────────────────────────────────────────────────────────────────
  $rows = [];
  foreach ($responses as $response) {
    $form = $forms[$response->form_id] ?? NULL;
    $form_title = $form ? check_plain($form->title) : t('Unknown form');
    $is_anon = $form && $form->anonymous_responses;
    $email = $is_anon ? t('Anonymous response') : check_plain($response->email ?: t('N/A'));
    $submitted = format_date($response->submitted_at, 'custom', 'M j Y, g:i a');

    $row = [$email, $form_title, $submitted];

    if ($any_editable) {
      $row[] = ($form && $form->editable && $response->updated_at)
        ? format_date($response->updated_at, 'custom', 'M j Y, g:i a')
        : t('—');
    }

    $row[] = _modularform_response_links($response->id);
    $rows[] = $row;
  }
  $show_form_filter = _modularform_is_site_admin() || _modularform_is_form_admin();

  // ── Build ───────────────────────────────────────────────────────────────────
  return [
    '#theme' => 'response_list',
    '#header' => $header,
    '#rows' => $rows,
    '#show_form_filter' => $show_form_filter,
    '#attached' => [
      'css' => [drupal_get_path('module', 'modularform') . '/css/response.css'],
      'js' => [
        [
          'data' => drupal_get_path('module', 'modularform') . '/js/modularform.response-filter.js',
          'type' => 'file',
          'weight' => 10,
        ],
        [
          'data' => [
            'modularform_autocomplete_base' => url('modularform'),
            'modularform_show_form_filter' => $show_form_filter,
          ],
          'type' => 'setting',
        ],
      ],
    ],
  ];
}

/**
 * Returns permitted operation links for a response.
 *
 * @param int $id Response ID.
 * @return array
 */
function _modularform_response_links($id) {
  $links = [];

  if (response_view_access($id)) {
    $links['view'] = [
      'title' => t('View'),
      'href'  => 'forms/response/view/' . $id,
    ];
  }

  if (response_edit_access($id)) {
    $links['edit'] = [
      'title' => t('Edit'),
      'href'  => 'forms/response/edit/' . $id,
    ];
  }

  if (response_delete_access($id)) {
    $links['delete'] = [
      'title' => t('Delete'),
      'href'  => 'forms/response/delete/' . $id,
    ];
  }

  return $links;
}

/**
 *
 * Helper function to format date
 *
 * @param $ymd
 * @param $boundary
 *
 * @return string
 */
function _modularform_solr_date($ymd, $boundary = 'start') {
  if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $ymd)) {
    return '*';
  }
  return $ymd . ($boundary === 'end' ? 'T23:59:59Z' : 'T00:00:00Z');
}

/**
 * AJAX callback: returns just the response table + pager as HTML.
 * Accepts the same GET params as modularform_response_list_page().
 */
function modularform_response_list_ajax() {
  // Reuse all the existing query-building logic by calling the page function
  // and pulling the rendered table fragment out of its render array.
  $build = modularform_response_list_page();

  // Render only the table theme + pager — no page wrapper, no filter bar.
  $output = theme('response_list', array(
    'header' => $build['#header'],
    'rows'   => $build['#rows'],
  ));

  $output .= theme('pager');

  drupal_json_output(array('html' => $output));
  drupal_exit();
}

function modularform_response_delete($response_id) {
  $response = db_select('modularform_responses', 'r')
    ->fields('r', ['id'])
    ->condition('r.id', $response_id)
    ->execute()
    ->fetchObject();
  if (!$response) {
    drupal_set_message(t('Response not found.'), 'error');
    drupal_goto('forms/response/list');
  }
  $transaction = db_transaction();
  try {
    _modularform_delete_response_data($response_id);
    drupal_set_message(t('Response and all its answers have been deleted.'));
  }
  catch (Exception $e) {
    $transaction->rollback();
    watchdog_exception('modularform', $e);
    drupal_set_message(t('An error occurred while deleting the response. Please try again.'), 'error');
  }
  drupal_goto('forms/response/list');
}

function _modularform_delete_response_data($response_id) {
  $answer_ids = db_select('modularform_answers', 'a')
    ->fields('a', ['id'])
    ->condition('a.response_id', $response_id)
    ->execute()
    ->fetchCol();

  if (!empty($answer_ids)) {
    // Delete files from disk and file_managed.
    $file_fids = db_select('modularform_answer_files', 'af')
      ->fields('af', ['fid'])
      ->condition('af.answer_id', $answer_ids, 'IN')
      ->execute()
      ->fetchCol();

    foreach ($file_fids as $fid) {
      $file = file_load($fid);
      if ($file) {
        file_usage_delete($file, 'modularform', 'response', $response_id);
        file_delete($file);
      }
    }

    db_delete('modularform_answer_files')
      ->condition('answer_id', $answer_ids, 'IN')
      ->execute();

    db_delete('modularform_answers_multi')
      ->condition('answer_id', $answer_ids, 'IN')
      ->execute();
  }

  db_delete('modularform_answers')
    ->condition('response_id', $response_id)
    ->execute();

  entity_delete('modularform_response', $response_id);
}

/**
 * Menu callback: modularform/filter/answer-values
 *
 * Returns distinct Solr values for a given question field as JSON.
 *
 * Query params:
 *   key     — the Solr field name, e.g. ss_val_q_6_6a05bb09ddc79
 *   form_id — the form ID to scope the facet to
 *   q       — optional search term to filter values
 *
 * Response:
 *   { results: [ { id: "Manager", label: "Manager" }, … ] }
 *   { results: [] }                        — no values found
 *   { error: "solr_unavailable" }          — Solr is down
 */
function modularform_filter_answer_values() {
  $solr_key = isset($_GET['key']) ? check_plain($_GET['key']) : '';
  $form_id = isset($_GET['form_id']) ? (int) $_GET['form_id'] : 0;
  $term = isset($_GET['term']) ? check_plain($_GET['term']) : '';

  // Basic validation — key must look like {type}_val_q_{id}_{hash}
  if (!$solr_key || !$form_id
    || !preg_match('/^[a-z]+_val_q_\d+_[a-z0-9]+$/', $solr_key)
    || !form_view_access($form_id)
  ) {
    drupal_json_output(['error' => 'access_denied']);
    drupal_exit();
  }

  module_load_include('inc', 'modularform_search', 'includes/modularform_response_search');
  $values = _modularform_filter_get_answer_values($solr_key, $form_id, $term);

  if (is_null($values)) {
    drupal_json_output(['error' => 'solr_unavailable']);
    drupal_exit();
  }

  $results = array_map(function($v) {
    $v = is_array($v) ? implode(', ', $v) : (string) $v; //  for sm multiple array check
    return ['id' => $v, 'label' => $v];
  }, $values);

  drupal_json_output(['results' => $results]);
  drupal_exit();
}

/**
 *
 * Get questions.
 *
 * @return void
 */
function modularform_filter_questions() {
  $form_id = isset($_GET['form_id']) ? (int) $_GET['form_id'] : 0;
  $configured_only = isset($_GET['configured']) ? (bool) $_GET['configured'] : FALSE;

  if (!$form_id || !form_view_access($form_id)) {
    drupal_json_output(['error' => 'access_denied']);
    drupal_exit();
  }

  $query = db_select('modularform_questions', 'q');
  $query->join('modularform_answers', 'a', 'a.q_id = q.q_id');
  $query->fields('q', ['q_id', 'label']);
  $query->addExpression('MIN(a.datatype)', 'datatype');
  $query->condition('q.form_id', $form_id);
  $query->groupBy('q.q_id');
  $query->groupBy('q.label');
  $query->orderBy('q.q_id');
  $query->range(0, 50);

  if ($configured_only) {
    $query->condition('q.is_filterable', 1);
  }

  $type_map = array_flip(modularform_type_map());
  $solr_field_map = solr_field_map();
  foreach ($query->execute() as $row) {
    $type_name = $type_map[$row->datatype] ?? NULL;
    $prefix = $solr_field_map[$type_name] ?? NULL;
    if (!$prefix) {
      continue;
    }
    $results[] = [
      'id'       => $prefix . '_' . $row->q_id,
      'label'    => $row->label,
      'datatype' => $type_name,
    ];
  }

  drupal_json_output(['results' => $results]);
  drupal_exit();
}

function modularform_filter_form_search() {
  $term    = isset($_GET['term']) ? check_plain($_GET['term']) : '';
  $form_id = isset($_GET['id'])   ? (int) $_GET['id']         : 0;

  // ── Single form lookup (restore from URL on page load) ──────────────────────
  if ($form_id) {
    if (!form_view_access($form_id)) {
      drupal_json_output(['results' => []]);
      drupal_exit();
    }

    $forms = entity_get_controller('modularform_form')->load([$form_id]);
    $form  = reset($forms);
    if (!$form) {
      drupal_json_output(['results' => []]);
      drupal_exit();
    }

    drupal_json_output([
      'results' => [['id' => (int) $form->id, 'label' => $form->title]],
    ]);
    drupal_exit();
  }

  // ── Title search via Solr ───────────────────────────────────────────────────
  if (!$term) {
    drupal_json_output(['results' => []]);
    drupal_exit();
  }

  module_load_include('inc', 'modularform_search', 'includes/modularform_forms_search');
  $rows = modularform_search_query_forms([
    'search' => $term,
    'status' => 1,
  ]);

  if (isset($rows['error'])) {
    drupal_json_output(['error' => 'solr_unavailable']);
    drupal_exit();
  }

  // ── Filter to only forms the current user can actually see ──────────────────
  $results = [];
  foreach ($rows as $row) {
    if (form_view_access($row->id)) {
      $results[] = ['id' => (int) $row->id, 'label' => $row->title];
    }
  }

  drupal_json_output(['results' => $results]);
  drupal_exit();
}
