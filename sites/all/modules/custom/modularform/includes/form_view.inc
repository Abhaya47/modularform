<?php

/* =============================================================================
   2. STATIC CACHE
   ============================================================================= */

/**
 * Load and statically cache the full form row.
 */
function _modularform_load($form_id) {
  $cache = &drupal_static(__FUNCTION__, []);

  if (!isset($cache[$form_id])) {
    $cache[$form_id] = db_select('modularform_forms', 'f')
      ->fields('f')
      ->condition('f.id', (int) $form_id)
      ->isNull('f.deleted_at')
      ->execute()
      ->fetchAssoc();
  }

  return $cache[$form_id];
}

/**
 * Load and statically cache questions for a form.
 */
function _modularform_load_questions($form_id) {
  $cache = &drupal_static(__FUNCTION__, []);

  if (!isset($cache[$form_id])) {
    $rows = db_select('modularform_questions', 'q')
      ->fields('q')
      ->condition('q.form_id', (int) $form_id)
      ->orderBy('q.position', 'ASC')
      ->execute()
      ->fetchAllAssoc('q_id', PDO::FETCH_ASSOC);

    $cache[$form_id] = array_values($rows);
  }

  return $cache[$form_id];
}

/**
 * Load and statically cache members (with display names) for a form.
 */
function _modularform_load_members($form_id) {
  $cache = &drupal_static(__FUNCTION__, []);

  if (!isset($cache[$form_id])) {
    $members = db_select('modularform_form_members', 'm')
      ->fields('m')
      ->condition('m.form_id', (int) $form_id)
      ->orderBy('m.roles', 'ASC')
      ->orderBy('m.created_at', 'ASC')
      ->execute()
      ->fetchAll(PDO::FETCH_ASSOC);

    if (!empty($members)) {
      $uids = array_column($members, 'uid');
      $users = user_load_multiple($uids);

      foreach ($members as &$m) {
        $uid = (int) $m['uid'];
        $m['name'] = isset($users[$uid]) ? format_username($users[$uid]) : '?';
      }
      unset($m);
    }

    $cache[$form_id] = $members;
  }

  return $cache[$form_id];
}

/* =============================================================================
   3. PAGE CALLBACK
   ============================================================================= */

function modularform_view_page($form_id) {
  $form_id = (int) $form_id;
  $form = _modularform_load($form_id);

  drupal_add_css(
    drupal_get_path('module', 'modularform') . '/css/modularform_view.css',
    ['group' => CSS_DEFAULT, 'every_page' => FALSE]
  );
  drupal_add_css(
    'https://cdn.jsdelivr.net/npm/@tabler/icons-webfont@latest/tabler-icons.min.css',
    ['type' => 'external', 'group' => CSS_DEFAULT]
  );
  drupal_add_js(
    drupal_get_path('module', 'modularform') . '/js/modularform_view.js',
    ['group' => JS_DEFAULT, 'every_page' => FALSE]
  );

  $questions = _modularform_load_questions($form_id);
  $members = _modularform_load_members($form_id);

  $filterable_form = drupal_get_form('modularform_filterable_form', $questions);

  module_load_include('inc', 'modularform_analytics', 'modularform_analytics.wordcloud');
  $tag_cloud = modularform_analytics_get_tag_counts($form_id);

  // ← Invoke the library hook
  $libraries = module_invoke('modularform_analytics', 'library');
  if (isset($libraries['wordcloud'])) {
    drupal_add_library('modularform_analytics', 'wordcloud');
  }

  drupal_add_js(
    ['modularformWordCloud' => ['words' => $tag_cloud]],
    'setting'
  );

  return theme('modularform_view_page', [
    'form' => $form,
    'questions' => $questions,
    'members' => $members,
    'stats' => _modularform_view_get_stats($form_id, $form, $members),
    'timeline' => _modularform_view_get_timeline($form_id),
    'tag_cloud' => $tag_cloud,
    'filterable_form' => $filterable_form,
    'form_url' => url('modularform/' . $form_id),
  ]);
}

/* =============================================================================
   4. DATA HELPERS
   ============================================================================= */

/**
 * Compute aggregate stats from already-loaded data where possible.
 * Only hits the DB for response-level aggregates.
 */
function _modularform_view_get_stats($form_id, array $form, array $members) {
  $total = count($members);
  $notified = 0;
  foreach ($members as $m) {
    if (!empty($m['notified'])) {
      $notified++;
    }
  }

  // Single query for all response aggregates.
  $agg = db_query(
    'SELECT COUNT(*) AS submitted,
            MAX(submitted_at) AS last_submitted_at,
            AVG(submitted_at - created_at) AS avg_seconds
     FROM {modularform_responses}
     WHERE form_id = :fid',
    [':fid' => $form_id]
  )->fetchAssoc();

  $days_left = NULL;
  if ($form['closes'] && $form['closes_at']) {
    $diff = (int) $form['closes_at'] - REQUEST_TIME;
    $days_left = $diff > 0 ? (int) ceil($diff / 86400) : 0;
  }

  $submitted = (int) $agg['submitted'];

  return [
    'total' => $total,
    'submitted' => $submitted,
    'pending' => $total - $submitted,
    'notified' => $notified,
    'last_submitted_at' => $agg['last_submitted_at'] ?: NULL,
    'avg_time_seconds' => $agg['avg_seconds'] ? (int) round($agg['avg_seconds']) : 0,
    'days_left' => $days_left,
  ];
}

/**
 * Build daily submission buckets for the last 30 days.
 */
function _modularform_view_get_timeline($form_id) {
  $n_buckets = 30;
  $bucket_size = 86400;
  $start = REQUEST_TIME - ($n_buckets * $bucket_size);

  $timestamps = db_select('modularform_responses', 'r')
    ->fields('r', ['submitted_at'])
    ->condition('r.form_id', (int) $form_id)
    ->condition('r.submitted_at', $start, '>=')
    ->execute()
    ->fetchCol();

  $buckets = [];
  for ($i = 0; $i < $n_buckets; $i++) {
    $buckets[$i] = [
      'label' => date('M j', $start + ($i * $bucket_size)),
      'count' => 0,
    ];
  }

  foreach ($timestamps as $ts) {
    $idx = (int) floor(((int) $ts - $start) / $bucket_size);
    if (isset($buckets[$idx])) {
      $buckets[$idx]['count']++;
    }
  }

  return array_values($buckets);
}

/* =============================================================================
   5. FILTERABLE QUESTIONS AJAX FORM
   ============================================================================= */

function modularform_filterable_form($form, &$form_state, array $questions) {
  $form['#prefix'] = '<div id="mfv-filterable-form-wrapper" class="mfv-filter-overlay" style="display:none">';
  $form['#suffix'] = '</div>';

  foreach ($questions as $q) {
    $q_id = check_plain($q['q_id']);
    $label = check_plain($q['label']);
    $type = check_plain($q['type']);

    $form['q_' . $q_id] = [
      '#type' => 'checkbox',
      '#title' => $label,
      '#default_value' => !empty($q['is_filterable']) ? 1 : 0,
      '#attributes' => [
        'data-label' => $label,
        'class' => ['mfv-overlay-checkbox'],
      ],
    ];
  }

  $form['actions'] = [
    '#type' => 'container',
    '#attributes' => ['class' => ['mfv-overlay-actions']],
  ];

  $form['actions']['cancel'] = [
    '#type' => 'button',
    '#value' => t('Cancel'),
    '#attributes' => [
      'id' => 'mfv-filter-cancel',
      'class' => ['mfv-btn', 'mfv-btn-sm'],
    ],
  ];

  $form['actions']['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save changes'),
    '#attributes' => ['class' => ['mfv-btn', 'mfv-btn-sm', 'mfv-btn-primary']],
  ];

  return $form;
}

function modularform_filterable_form_submit($form, &$form_state) {
  $questions = $form_state['build_info']['args'][0];
  $form_id = !empty($questions[0]['form_id']) ? (int) $questions[0]['form_id'] : 0;

  foreach ($form_state['values'] as $key => $value) {
    if (strpos($key, 'q_') !== 0) {
      continue;
    }
    $q_id = substr($key, 2);

    db_update('modularform_questions')
      ->fields([
        'is_filterable' => (int) (bool) $value,
        'updated_at' => REQUEST_TIME,
      ])
      ->condition('q_id', $q_id)
      ->condition('form_id', $form_id)
      ->execute();
  }

  drupal_static_reset('_modularform_load_questions');
  drupal_set_message(t('Filterable questions updated.'));
  $form_state['redirect'] = 'forms/' . 'view/' . $form_id;
}
