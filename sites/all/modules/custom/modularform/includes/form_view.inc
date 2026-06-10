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

  if (isset($cache[$form_id])) {
    return $cache[$form_id];
  }

  // Confirmed Drupal members
  $members = db_select('modularform_form_members', 'm')
    ->fields('m')
    ->condition('m.form_id', (int) $form_id)
    ->condition('m.uid', 0, '>')
    ->orderBy('m.roles', 'ASC')
    ->orderBy('m.created_at', 'ASC')
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  if (!empty($members)) {
    $uids = array_column($members, 'uid');
    $users = user_load_multiple($uids);
    foreach ($members as &$m) {
      $uid = (int) $m['uid'];
      $m['name']  = isset($users[$uid]) ? format_username($users[$uid]) : '?';
      $m['email'] = isset($users[$uid]) ? $users[$uid]->mail : '';
      $m['kind']  = 'member';  // confirmed Drupal user
    }
    unset($m);
  }

  // Pending invites (not yet accepted)
  $invites = db_select('modularform_form_invites', 'i')
    ->fields('i', ['email', 'token', 'notified', 'created_at'])
    ->condition('i.form_id', (int) $form_id)
    ->condition('i.status', 0)  // 0 = pending only
    ->orderBy('i.created_at', 'ASC')
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  foreach ($invites as &$inv) {
    $inv['kind']   = 'invite';   // external, not yet registered
    $inv['name']   = $inv['email'];
    $inv['roles']  = 2;          // participant by intent
    $inv['status'] = 0;          // always pending — hasn't submitted anything
    $inv['uid']    = 0;
  }
  unset($inv);

  $cache[$form_id] = array_merge($members, $invites);
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

  $questions  = _modularform_load_questions($form_id);
  $members    = _modularform_load_members($form_id);
  $stats      = _modularform_view_get_stats($form_id, $form, $members);
  $timeline   = _modularform_view_get_timeline($form_id);

  $filterable_form = drupal_get_form('modularform_filterable_form', $questions);

  $tag_cloud = [];

  if (module_exists('modularform_analytics')) {
    module_load_include('inc', 'modularform_analytics', 'modularform_analytics.wordcloud');
    $tag_cloud = modularform_analytics_get_tag_counts($form_id);

    $libraries = module_invoke('modularform_analytics', 'library');
    if (isset($libraries['wordcloud'])) {
      drupal_add_library('modularform_analytics', 'wordcloud');
    }
    drupal_add_js(['modularformWordCloud' => ['words' => $tag_cloud]], 'setting');
  }

  // ── Labels / display maps ─────────────────────────────────────────────────
  $status_labels        = ['Drafted', 'Published'];
  $status_classes       = ['mfv-badge-amber', 'mfv-badge-green'];
  $visibility_labels    = ['Public', 'Restricted', 'Private'];
  $visibility_icons     = ['ti-world', 'ti-lock-open', 'ti-lock'];
  $role_labels          = ['Creator', 'Viewer', 'Participant'];
  $member_status_labels  = ['Pending', 'Submitted'];
  $member_status_classes = ['mfv-badge-amber', 'mfv-badge-green'];

  // ── Formatted dates ───────────────────────────────────────────────────────
  $created_date   = !empty($form['created_at'])     ? date('M j, Y',       $form['created_at'])     : '—';
  $closes_date    = !empty($form['closes_at'])      ? date('M j, Y g:i A', $form['closes_at'])      : '—';
  $scheduled_date = !empty($form['scheduled_date']) ? date('M j, Y g:i A', $form['scheduled_date']) : NULL;
  $last_response  = !empty($stats['last_submitted_at'])
    ? format_interval(REQUEST_TIME - $stats['last_submitted_at'], 1) . ' ago'
    : 'No responses yet';

  // ── Derived response counts ───────────────────────────────────────────────
  $submitted = (int) $stats['submitted'];
  $total     = (int) $stats['total'];
  $pending   = $total - $submitted;
  $pct       = $total > 0 ? round($submitted / $total * 100) : 0;

  // ── Donut arc values ──────────────────────────────────────────────────────
  $C             = 314.16;
  $submitted_arc = $total > 0 ? round($C * ($submitted / $total), 2) : 0;
  $pending_arc   = round($C - $submitted_arc, 2);

  // ── Filterable question count ─────────────────────────────────────────────
  $filterable_count = 0;
  foreach ($questions as $q) {
    if (!empty($q['is_filterable'])) $filterable_count++;
  }

  // ── Timeline bar chart max ────────────────────────────────────────────────
  $max_count = 1;
  foreach ($timeline as $bucket) {
    if ((int) $bucket['count'] > $max_count) $max_count = (int) $bucket['count'];
  }

  $invite_count = 0;
  foreach ($members as $m) {
    if ($m['kind'] === 'invite') $invite_count++;
  }

  // ── Resolved URLs ─────────────────────────────────────────────────────────
  $token_delete = drupal_get_token('modularform_delete_' . $form_id);
  $token_export = drupal_get_token('modularform_export');

  return theme('modularform_view_page', [
    'form'            => $form,
    'questions'       => $questions,
    'members'         => $members,
    'stats'           => $stats,
    'timeline'        => $timeline,
    'tag_cloud'       => $tag_cloud,
    'filterable_form' => $filterable_form,

    'display' => [
      'status_labels'         => $status_labels,
      'status_classes'        => $status_classes,
      'visibility_labels'     => $visibility_labels,
      'visibility_icons'      => $visibility_icons,
      'role_labels'           => $role_labels,
      'member_status_labels'  => $member_status_labels,
      'member_status_classes' => $member_status_classes,
      'created_date'          => $created_date,
      'closes_date'           => $closes_date,
      'closed' => _modularform_is_closed($form),
      'scheduled_date'        => $scheduled_date,
      'last_response'         => $last_response,
    ],

    'counts' => [
      'submitted'        => $submitted,
      'total'            => $total,
      'pending'          => $pending,
      'pct'              => $pct,
      'filterable_count' => $filterable_count,
      'max_count'        => $max_count,
      'invite_count'     => $invite_count,
    ],

    'donut' => [
      'C'             => $C,
      'submitted_arc' => $submitted_arc,
      'pending_arc'   => $pending_arc,
    ],

    'urls' => [
      'preview'  => url('forms/' . $form_id . '/builder/preview'),
      'edit'     => url('forms/' . $form_id . '/builder'),
      'settings' => url('forms/edit/' . $form_id),
      'members'  => url('forms/edit/' . $form_id, ['fragment' => 'edit-visibility']),
      'invites'  => url('forms/edit/' . $form_id, ['fragment' => 'edit-visibility']),
      'export'   => url('modularform/export/page', ['query' => ['fids' => $form_id, 'token' => $token_export]]),
      'delete'   => url('forms/delete/' . $form_id, ['query' => ['token' => $token_delete]]),
    ],
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
  if (!empty($form['closes_at'])) {
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

  $on = $off = [];
  foreach ($form_state['values'] as $key => $value) {
    if (strpos($key, 'q_') !== 0) continue;
    $q_id = substr($key, 2);
    if ((int)(bool)$value) { $on[] = $q_id; } else { $off[] = $q_id; }
  }
  if ($on) {
    db_update('modularform_questions')
      ->fields(['is_filterable' => 1, 'updated_at' => REQUEST_TIME])
      ->condition('q_id', $on, 'IN')->condition('form_id', $form_id)->execute();
  }
  if ($off) {
    db_update('modularform_questions')
      ->fields(['is_filterable' => 0, 'updated_at' => REQUEST_TIME])
      ->condition('q_id', $off, 'IN')->condition('form_id', $form_id)->execute();
  }

  drupal_static_reset('_modularform_load_questions');
  drupal_set_message(t('Filterable questions updated.'));
  $form_state['redirect'] = 'forms/' . 'view/' . $form_id;
}
