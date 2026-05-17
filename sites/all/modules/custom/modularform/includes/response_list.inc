<?php

module_load_include('inc', 'modularform', 'modularform.rules');

function modularform_response_list_page() {
  $form_id_filter = isset($_GET['form_id']) ? (int) $_GET['form_id'] : NULL;
  $pair_filters   = isset($_GET['filters']) && is_array($_GET['filters'])
    ? array_values($_GET['filters'])
    : [];

  // ── Build base query ───────────────────────────────────────────────────────
  $query = db_select('modularform_responses', 'r')
    ->fields('r', ['id', 'form_id', 'email', 'submitted_at', 'updated_at'])
    ->orderBy('r.submitted_at', 'DESC');

  // ── Form filter ────────────────────────────────────────────────────────────
  if ($form_id_filter) {
    $query->condition('r.form_id', $form_id_filter);
  }

  // ── Pair filters via Solr ──────────────────────────────────────────────────
  if ($form_id_filter && !empty($pair_filters)) {
    $fq = [
      'bundle:modularform_response',
      'is_form_id:' . $form_id_filter,
    ];

    foreach ($pair_filters as $pair) {
      if (!empty($pair['solr_key']) && isset($pair['answer'])) {
        $fq[] = $pair['solr_key'] . ':"' . addslashes($pair['answer']) . '"';
      }
    }

    $params = [
      'fq'   => $fq,
      'fl'   => 'entity_id',
      'rows' => 500,
    ];

    $docs = _modularform_search_solr_query('*:*', $params);

    if ($docs === NULL) {
      // Solr down — show unfiltered form results with a warning
      drupal_set_message(t('Filter service temporarily unavailable. Showing unfiltered results.'), 'warning');
    }
    elseif (empty($docs)) {
      // Solr healthy, zero matches — force empty result set
      $query->condition('r.id', [0], 'IN');
    }
    else {
      $ids = array_map(function ($doc) { return (int) $doc->entity_id; }, $docs);
      $query->condition('r.id', $ids, 'IN');
    }
  }

  // ── Load form metadata for display ────────────────────────────────────────
  // Only load the form(s) we actually need rather than all forms
  $forms = [];
  if ($form_id_filter) {
    $loaded = entity_get_controller('modularform_form')->load([$form_id_filter]);
    $form   = reset($loaded);
    if ($form) {
      $forms[$form->id] = $form;
    }
  }
  else {
    $forms = db_select('modularform_forms', 'f')
      ->fields('f', ['id', 'title', 'editable', 'anonymous_responses'])
      ->execute()
      ->fetchAllAssoc('id');
  }

  if (empty($forms) && !$form_id_filter) {
    return '<p>' . t('No forms found.') . '</p>';
  }

  $any_editable = FALSE;
  foreach ($forms as $f) {
    if ($f->editable) { $any_editable = TRUE; break; }
  }

  // ── Pager ──────────────────────────────────────────────────────────────────
  $query    = $query->extend('PagerDefault')->limit(25);
  $responses = $query->execute()->fetchAll();

  // ── Header ─────────────────────────────────────────────────────────────────
  $header = [t('Respondent'), t('Form'), t('Submitted')];
  if ($any_editable) { $header[] = t('Updated'); }
  $header[] = t('Operations');

  // ── Rows ───────────────────────────────────────────────────────────────────
  $rows = [];
  foreach ($responses as $response) {
    $form       = isset($forms[$response->form_id]) ? $forms[$response->form_id] : NULL;
    $form_title = $form ? check_plain($form->title) : t('Unknown form');
    $is_anon    = $form && $form->anonymous_responses;
    $email      = $is_anon ? t('Anonymous response') : check_plain($response->email ?: t('N/A'));
    $submitted  = format_date($response->submitted_at, 'custom', 'M j Y, g:i a');

    $row = [$email, $form_title, $submitted];

    if ($any_editable) {
      $row[] = ($form && $form->editable && $response->updated_at)
        ? format_date($response->updated_at, 'custom', 'M j Y, g:i a')
        : t('—');
    }

    $links = [
      'view'   => ['title' => t('View'),   'href' => 'forms/response/view/'   . $response->id],
      'delete' => ['title' => t('Delete'), 'href' => 'forms/response/delete/' . $response->id],
    ];
    if ($any_editable) {
      $links['edit'] = ['title' => t('Edit'), 'href' => 'forms/response/edit/' . $response->id];
    }

    $row[] = $links;
    $rows[] = $row;
  }

  return [
    '#theme'  => 'response_list',
    '#header' => $header,
    '#rows'   => $rows,
    '#attached' => [
      'css' => [drupal_get_path('module', 'modularform') . '/css/response.css'],
      'js'  => [
        [
          'data' => drupal_get_path('module', 'modularform') . '/js/modularform.response-filter.js',
          'type'   => 'file',
          'weight' => 10,
        ],
        [
          'data' => ['modularform_autocomplete_base' => url('modularform')],
          'type' => 'setting',
        ],
      ],
    ],
  ];
}

function modularform_response_delete($response_id) {
  // Validate the response exists
  $response = db_select('modularform_responses', 'r')
    ->fields('r', ['id'])
    ->condition('r.id', $response_id)
    ->execute()
    ->fetchObject();

  if (!$response) {
    drupal_set_message(t('Response not found.'), 'error');
    drupal_goto('forms/response/list');
  }

  // Get all answer IDs for this response (needed to delete multi-values)
  $answer_ids = db_select('modularform_answers', 'a')
    ->fields('a', ['id'])
    ->condition('a.response_id', $response_id)
    ->execute()
    ->fetchCol();

  $transaction = db_transaction();

  try {
    // 1. Delete multi-value answers if any answers exist
    if (!empty($answer_ids)) {
      db_delete('modularform_answers_multi')
        ->condition('answer_id', $answer_ids, 'IN')
        ->execute();
    }

    // 2. Delete answers
    db_delete('modularform_answers')
      ->condition('response_id', $response_id)
      ->execute();

    // 3. Delete the response itself
    db_delete('modularform_responses')
      ->condition('id', $response_id)
      ->execute();

    drupal_set_message(t('Response and all its answers have been deleted.'));
  }
  catch (Exception $e) {
    $transaction->rollback();
    watchdog_exception('modularform', $e);
    drupal_set_message(t('An error occurred while deleting the response. Please try again.'), 'error');
  }

  drupal_goto('forms/response/list');
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
  $solr_key = isset($_GET['key'])     ? check_plain($_GET['key'])     : '';
  $form_id  = isset($_GET['form_id']) ? (int) $_GET['form_id']        : 0;
  $term = isset($_GET['term']) ? check_plain($_GET['term']) : '';

  // Basic validation — key must look like ss_val_q_{id}_{hash}
  if (!$solr_key || !$form_id || !preg_match('/^ss_val_q_\d+_[a-z0-9]+$/', $solr_key)) {
    drupal_json_output(['error' => 'invalid_params']);
    drupal_exit();
  }

  $values = _modularform_filter_get_answer_values($solr_key, $form_id, $term);

  if (is_null($values)) {
    drupal_json_output(['error' => 'solr_unavailable']);
    drupal_exit();
  }

  // Shape into { id, label } pairs the JS expects
  $results = array_map(function ($v) {
    return ['id' => $v, 'label' => $v];
  }, $values);

  drupal_json_output(['results' => $results]);
  drupal_exit();
}

function modularform_filter_questions() {
  $form_id = isset($_GET['form_id']) ? (int) $_GET['form_id'] : 0;

  if (!$form_id) {
    drupal_json_output(['error' => 'invalid_params']);
    drupal_exit();
  }

  $query = db_select('modularform_questions', 'q');
  $query->join('modularform_answers', 'a', 'a.q_id = q.q_id');
  $query = db_select('modularform_questions', 'q');
  $query->join('modularform_answers', 'a', 'a.q_id = q.q_id');
  $query->fields('q', ['q_id', 'label']);
  $query->addExpression('MIN(a.datatype)', 'datatype');  // ← separate line, not chained
  $query->condition('q.form_id', $form_id);
  $query->groupBy('q.q_id');
  $query->groupBy('q.label');
  $query->orderBy('q.q_id');
  $query->range(0, 50);

  $type_map       = array_flip(modularform_type_map());
  $datatype_map   = modularform_datatype_map();
  $solr_field_map = solr_field_map();

  $results = [];
  foreach ($query->execute() as $row) {
    $type_name = isset($type_map[$row->datatype])   ? $type_map[$row->datatype]   : NULL;
    $storage   = isset($datatype_map[$type_name])   ? $datatype_map[$type_name]   : NULL;
    $prefix    = isset($solr_field_map[$storage])   ? $solr_field_map[$storage]   : NULL;

    if (!$prefix || $storage !== 'varchar') {
      continue;
    }

    $results[] = [
      'id'    => $prefix . '_' . $row->q_id,
      'label' => $row->label,
    ];
  }

  drupal_json_output(['results' => $results]);
  drupal_exit();
}

function modularform_filter_form_search() {
  $term = isset($_GET['term']) ? check_plain($_GET['term']) : '';
  $form_id = isset($_GET['id']) ? (int) $_GET['id']             : 0;

  // Single form lookup — restore from URL on page load
  if ($form_id) {
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

  // Title search via Solr
  if (!$term) {
    drupal_json_output(['results' => []]);
    drupal_exit();
  }
  module_load_include('inc', 'modularform_search', 'includes/modularform_search_forms');
  $rows = modularform_search_query_forms([
    'search' => $term,
    'status' => 1,        // published only
  ]);

  if (isset($rows['error'])) {
    drupal_json_output(['error' => 'solr_unavailable']);
    drupal_exit();
  }

  $results = array_map(function ($row) {
    return ['id' => (int) $row->id, 'label' => $row->title];
  }, $rows);

  drupal_json_output(['results' => $results]);
  drupal_exit();
}
