<?php

function modularform_view_page($form_id) {
  $form_id = (int) $form_id;

  // ── Load form record ──────────────────────────────────────────────────────
  $form = db_select('modularform_forms', 'f')
    ->fields('f')
    ->condition('f.id', $form_id)
    ->isNull('f.deleted_at')
    ->execute()
    ->fetchAssoc();

  if (!$form) {
    return drupal_not_found();
  }

  // ── Timeline period (default: day) ────────────────────────────────────────
  $allowed_periods = ['hour', 'day', 'week', 'month'];
  $period = isset($_GET['period']) && in_array($_GET['period'], $allowed_periods)
    ? $_GET['period']
    : 'day';

  // ── Attach CSS ────────────────────────────────────────────────────────────
  drupal_add_css(
    drupal_get_path('module', 'modularform') . '/css/modularform_view.css',
    ['group' => CSS_DEFAULT, 'every_page' => FALSE]
  );

  // Tabler icons CDN (outline only)
  drupal_add_css(
    'https://cdn.jsdelivr.net/npm/@tabler/icons-webfont@latest/tabler-icons.min.css',
    ['type' => 'external', 'group' => CSS_DEFAULT]
  );

  // ── Load data ─────────────────────────────────────────────────────────────
  $questions = _modularform_view_get_questions($form_id);
  $members   = _modularform_view_get_members($form_id);
  $stats     = _modularform_view_get_stats($form_id, $members);
  $timeline  = _modularform_view_get_timeline($form_id, $period);

  // ── Build AJAX filterable form ────────────────────────────────────────────
  $filterable_form = drupal_render(
    drupal_get_form('modularform_filterable_form', $form_id, $questions)
  );

  // ── Build template variables ──────────────────────────────────────────────
  $variables = [
    'form'            => $form,
    'questions'       => $questions,
    'members'         => $members,
    'stats'           => $stats,
    'timeline'        => $timeline,
    'timeline_period' => $period,
    'filterable_form' => $filterable_form,
    'form_url'        => url('modularform/' . $form_id),
  ];

  // ── Attach JS ─────────────────────────────────────────────────────────────
  drupal_add_js(
    drupal_get_path('module', 'modularform') . '/js/modularform_view.js',
    ['group' => JS_DEFAULT, 'every_page' => FALSE]
  );

  return theme('modularform_view_page', $variables);
}

/* =============================================================================
   3. DATA HELPERS
   ============================================================================= */

/**
 * Load questions for a form, ordered by position.
 */
function _modularform_view_get_questions($form_id) {
  // Map integer type codes to human-readable labels.
  // Adjust this map to match your actual type constants.
  $type_labels = [
    0 => 'text',
    1 => 'textarea',
    2 => 'radio',
    3 => 'checkbox',
    4 => 'select',
    5 => 'date',
    6 => 'number',
    7 => 'email',
  ];

  $rows = db_select('modularform_questions', 'q')
    ->fields('q')
    ->condition('q.form_id', $form_id)
    ->orderBy('q.position', 'ASC')
    ->execute()
    ->fetchAllAssoc('q_id', PDO::FETCH_ASSOC);

  foreach ($rows as &$row) {
    $row['type'] = isset($type_labels[(int) $row['type']])
      ? $type_labels[(int) $row['type']]
      : 'input';
  }

  return array_values($rows);
}

/**
 * Load form members with their Drupal user display names.
 */
function _modularform_view_get_members($form_id) {
  return db_select('modularform_form_members', 'm')
    ->fields('m')
    ->condition('m.form_id', $form_id)
    ->orderBy('m.roles', 'ASC')
    ->orderBy('m.created_at', 'ASC')
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  // Attach display names.
  // (Drupal's user_load_multiple is fine for small member lists;
  //  for large forms consider a single JOIN instead.)
  $uids = array_column($members, 'uid');
  $users = user_load_multiple($uids);

  foreach ($members as &$m) {
    $uid = (int) $m['uid'];
    $m['name'] = isset($users[$uid]) ? format_username($users[$uid]) : '?';
  }

  return $members;
}

/**
 * Compute aggregate stats from modularform_responses and modularform_form_members.
 */
function _modularform_view_get_stats($form_id, array $members) {
  // Total members (participants + creator counts as submitted automatically)
  $total = count($members);

  // Submitted count
  $submitted = db_select('modularform_responses', 'r')
    ->condition('r.form_id', $form_id)
    ->countQuery()
    ->execute()
    ->fetchField();

  // Notified count
  $notified = 0;
  foreach ($members as $m) {
    if (!empty($m['notified'])) $notified++;
  }

  // Last submitted_at
  $last_submitted_at = db_select('modularform_responses', 'r')
    ->fields('r', ['submitted_at'])
    ->condition('r.form_id', $form_id)
    ->orderBy('r.submitted_at', 'DESC')
    ->range(0, 1)
    ->execute()
    ->fetchField();

  // Average time to complete (submitted_at - created_at per response)
  $avg_result = db_query(
    'SELECT AVG(submitted_at - created_at) FROM {modularform_responses} WHERE form_id = :fid',
    [':fid' => $form_id]
  )->fetchField();

  // Days left until form closes
  $form = db_select('modularform_forms', 'f')
    ->fields('f', ['closes', 'closes_at'])
    ->condition('f.id', $form_id)
    ->execute()
    ->fetchAssoc();

  $days_left = null;
  if ($form && $form['closes'] && $form['closes_at']) {
    $diff = (int) $form['closes_at'] - REQUEST_TIME;
    $days_left = $diff > 0 ? ceil($diff / 86400) : 0;
  }

  return [
    'total'              => $total,
    'submitted'          => (int) $submitted,
    'pending'            => $total - (int) $submitted,
    'notified'           => $notified,
    'last_submitted_at'  => $last_submitted_at ?: null,
    'avg_time_seconds'   => $avg_result ? round($avg_result) : 0,
    'days_left'          => $days_left,
  ];
}

/**
 * Build timeline buckets for the bar chart.
 *
 * @param int    $form_id
 * @param string $period  hour|day|week|month
 * @return array  [ ['label' => string, 'count' => int], ... ]
 */
function _modularform_view_get_timeline($form_id, $period = 'day') {
  // Determine window and grouping.
  switch ($period) {
    case 'hour':
      $window      = 24 * 3600;          // last 24 hours
      $bucket_size = 3600;               // 1 hour buckets
      $label_fmt   = 'H:i';
      $n_buckets   = 24;
      break;

    case 'week':
      $window      = 12 * 7 * 86400;    // last 12 weeks
      $bucket_size = 7 * 86400;
      $label_fmt   = 'M j';
      $n_buckets   = 12;
      break;

    case 'month':
      $window      = 12 * 30 * 86400;   // last 12 months
      $bucket_size = 30 * 86400;
      $label_fmt   = 'M Y';
      $n_buckets   = 12;
      break;

    default: // day
      $window      = 30 * 86400;        // last 30 days
      $bucket_size = 86400;
      $label_fmt   = 'M j';
      $n_buckets   = 30;
      break;
  }

  $now   = REQUEST_TIME;
  $start = $now - $window;

  // Fetch raw timestamps in window
  $rows = db_select('modularform_responses', 'r')
    ->fields('r', ['submitted_at'])
    ->condition('r.form_id', $form_id)
    ->condition('r.submitted_at', $start, '>=')
    ->execute()
    ->fetchCol();

  // Build empty buckets
  $buckets = [];
  for ($i = 0; $i < $n_buckets; $i++) {
    $ts = $start + ($i * $bucket_size);
    $buckets[$i] = [
      'ts'    => $ts,
      'label' => date($label_fmt, $ts),
      'count' => 0,
    ];
  }

  // Distribute timestamps into buckets
  foreach ($rows as $ts) {
    $idx = (int) floor(($ts - $start) / $bucket_size);
    if (isset($buckets[$idx])) {
      $buckets[$idx]['count']++;
    }
  }

  return array_values($buckets);
}

/* =============================================================================
   4. FILTERABLE QUESTIONS FORM (AJAX)
   ============================================================================= */

/**
 * Form builder for the filterable questions overlay.
 *
 * Rendered inside the tpl.php and submitted via Drupal AJAX.
 */
function modularform_filterable_form($form, &$form_state, $form_id, array $questions) {
  $form['#attributes']['class'][] = 'mfv-filterable-form';
  $form['form_id'] = ['#type' => 'hidden', '#value' => $form_id];

  // ── Client-side search field (no Drupal submit, JS only) ──────────────────
  $form['question_search'] = [
    '#type'        => 'textfield',
    '#title'       => '',
    '#attributes'  => [
      'placeholder' => t('Search questions…'),
      'id'          => 'mfv-overlay-search',
      'autocomplete'=> 'off',
    ],
    '#prefix' => '<div class="mfv-overlay-search-wrap">',
    '#suffix' => '</div>',
  ];

  // ── Checkbox list ─────────────────────────────────────────────────────────
  $form['questions_wrap'] = [
    '#type'   => 'container',
    '#attributes' => ['id' => 'mfv-overlay-questions'],
  ];

  foreach ($questions as $q) {
    $q_id  = check_plain($q['q_id']);
    $label = check_plain($q['label']);
    $type  = check_plain($q['type']);

    $form['questions_wrap']['q_' . $q_id] = [
      '#type'          => 'checkbox',
      '#title'         => $label . ' <span class="mfv-overlay-type">' . $type . '</span>',
      '#default_value' => !empty($q['is_filterable']) ? 1 : 0,
      '#attributes'    => [
        'data-qid'   => $q_id,
        'data-label' => $label,
        'class'      => ['mfv-overlay-checkbox'],
      ],
      '#prefix' => '<div class="mfv-overlay-q-row">',
      '#suffix' => '</div>',
    ];
  }

  // ── Actions ───────────────────────────────────────────────────────────────
  $form['actions'] = [
    '#type'       => 'container',
    '#attributes' => ['class' => ['mfv-overlay-actions']],
  ];

  $form['actions']['cancel'] = [
    '#type'       => 'button',
    '#value'      => t('Cancel'),
    '#attributes' => ['id' => 'mfv-filter-cancel', 'class' => ['mfv-btn', 'mfv-btn-sm']],
    // Not a submit — handled purely by JS to close the overlay.
  ];

  $form['actions']['submit'] = [
    '#type'   => 'submit',
    '#value'  => t('Save changes'),
    '#attributes' => ['class' => ['mfv-btn', 'mfv-btn-sm', 'mfv-btn-primary']],
    '#ajax'   => [
      'callback' => 'modularform_filterable_form_ajax_callback',
      'wrapper'  => 'mfv-filter-overlay',
      'method'   => 'replace',
      'effect'   => 'fade',
    ],
  ];

  return $form;
}

/**
 * Validation: nothing to validate beyond token.
 */
function modularform_filterable_form_validate($form, &$form_state) {
  // Intentionally empty.
}

/**
 * Submit handler: persist is_filterable changes.
 */
function modularform_filterable_form_submit($form, &$form_state) {
  $form_id = (int) $form_state['values']['form_id'];

  // Collect all q_* values from submitted form state.
  foreach ($form_state['values'] as $key => $value) {
    if (strpos($key, 'q_') !== 0) {
      continue;
    }

    // Key is q_{q_id} — strip prefix to get the real q_id.
    $q_id        = substr($key, 2);
    $is_filterable = (int) (bool) $value;

    db_update('modularform_questions')
      ->fields([
        'is_filterable' => $is_filterable,
        'updated_at'    => REQUEST_TIME,
      ])
      ->condition('q_id', $q_id)
      ->condition('form_id', $form_id)
      ->execute();
  }

  drupal_set_message(t('Filterable questions updated.'));
}

/**
 * AJAX callback: return the updated form region.
 *
 * We re-render the full overlay so the checkboxes reflect the saved state.
 */
function modularform_filterable_form_ajax_callback($form, &$form_state) {
  // Re-fetch questions with updated is_filterable flags.
  $form_id   = (int) $form_state['values']['form_id'];
  $questions = _modularform_view_get_questions($form_id);

  // Return fresh form render array targeted at the overlay wrapper.
  return drupal_render(
    drupal_get_form('modularform_filterable_form', $form_id, $questions)
  );
}
