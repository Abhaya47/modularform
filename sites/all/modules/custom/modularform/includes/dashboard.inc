<?php

/* ──────────────────────────────────────────────────────────
   2. PAGE CALLBACK  (includes/dashboard.inc)
   ────────────────────────────────────────────────────────── */

const MODULARFORM_PER_PAGE = 25; // change here only when testing

function form_dashboard_page() {
  global $user;

  $module_path = drupal_get_path('module', 'modularform');
  $build['#attached']['css'][] = $module_path . '/css/dashboard.css';
  $build['#attached']['js'][] = $module_path . '/js/dashboard.js';

  $build['#attached']['js'][] = [
    'type' => 'setting',
    'data' => [
      'modularform' => [
        'ajaxUrl' => url('forms/ajax/search'),
        'isAdmin' => (int) _modularform_is_site_admin(),
        'currentUid' => (int) $user->uid,
        'token' => drupal_get_token('modularform_dashboard'),
        'perPage' => MODULARFORM_PER_PAGE,
      ],
    ],
  ];

  // Initial render — first page, no filters, no PagerDefault needed
  $forms = _modularform_dashboard_sql([], $user, 'created_desc', MODULARFORM_PER_PAGE, 0);

  $header = [
    t('Name'),
    t('Responses'),
    t('Owner'),
    t('Status'),
    t('Created'),
    t('Operations'),
  ];

  $uids = array_unique(array_column((array) $forms, 'created_by'));
  $users = user_load_multiple($uids);
  $rows = [];

  foreach ($forms as $form) {
    $owner = isset($users[$form->created_by])
      ? format_username($users[$form->created_by]) : t('Unknown');
    $status = $form->status ? t('Published') : t('Draft');
    $created = format_date($form->created_at, 'custom', 'M j Y');

    $links = [
      'settings_edit' => [
        'title' => t('Edit Settings'),
        'href' => 'forms/edit/' . $form->id,
      ],
      'builder_preview' => [
        'title' => t('Preview Survey'),
        'href' => 'forms/' . $form->id . '/builder/preview',
      ],
      'builder_edit' => [
        'title' => t('Edit Survey'),
        'href' => 'forms/' . $form->id . '/builder/',
      ],
      'delete' => [
        'title' => t('Delete Survey'),
        'href' => 'forms/delete/' . $form->id,
      ],
    ];

    $rows[] = [
      check_plain($form->title),
      check_plain($owner),
      $status,
      $created,
      isset($form->response_count) ? (int) $form->response_count : 0,  // NEW
      $links,
      (int) $form->id,
    ];
  }

  $build['dashboard'] = [
    '#theme' => 'dashboard',
    '#header' => $header,
    '#rows' => $rows,
    '#pager' => NULL,   // JS renders the pager, not Drupal
  ];

  return $build;
}

/* ──────────────────────────────────────────────────────────
   AJAX ENDPOINT  →  forms/ajax/search  (POST)
   ────────────────────────────────────────────────────────── */

function modularform_dashboard_ajax() {
  $token = isset($_POST['token']) ? $_POST['token'] : '';
  if (!drupal_valid_token($token, 'modularform_dashboard')) {
    drupal_json_output(['error' => 'invalid_token']);
    drupal_exit();
  }

  global $user;

  $search = isset($_POST['search']) ? check_plain(trim($_POST['search'])) : '';
  $status = (isset($_POST['status']) && $_POST['status'] !== '') ? (int) $_POST['status'] : NULL;
  $visibility = (isset($_POST['visibility']) && $_POST['visibility'] !== '') ? (int) $_POST['visibility'] : NULL;
  $sort = isset($_POST['sort']) ? check_plain($_POST['sort']) : 'created_desc';
  $owner_only = (isset($_POST['owner_only']) && $_POST['owner_only'] === '1');
  $page = isset($_POST['page']) ? max(0, (int) $_POST['page']) : 0;

  $conditions = [];
  if ($search !== '') {
    $conditions['search'] = $search;
  }
  if ($status !== NULL) {
    $conditions['status'] = $status;
  }
  if ($visibility !== NULL) {
    $conditions['visibility'] = $visibility;
  }
  if ($owner_only) {
    $conditions['created_by'] = $user->uid;
  }

  $limit = MODULARFORM_PER_PAGE;
  $total = _modularform_dashboard_count($conditions, $user);
  $forms = _modularform_dashboard_fetch($conditions, $user, $sort, $page, $limit);

  $uids = array_unique(array_column((array) $forms, 'created_by'));
  $users = user_load_multiple($uids);
  $rows = [];

  foreach ($forms as $form) {
    $owner = isset($users[$form->created_by])
      ? format_username($users[$form->created_by]) : t('Unknown');
    $rows[] = [
      'id' => (int) $form->id,
      'title' => check_plain($form->title),
      'owner' => check_plain($owner),
      'status' => $form->status ? t('Published') : t('Draft'),
      'status_val' => (int) $form->status,
      'created' => format_date($form->created_at, 'custom', 'M j Y'),
      'response_count' => isset($form->response_count) ? (int) $form->response_count : 0,
      'urls' => [
        'settings' => url('forms/edit/' . $form->id),
        'preview' => url('forms/' . $form->id . '/builder/preview'),
        'edit' => url('forms/' . $form->id . '/builder/'),
        'delete' => url('forms/delete/' . $form->id),
      ],
    ];
  }

  drupal_json_output([
    'rows' => $rows,
    'total' => $total,
    'page' => $page,
    'limit' => $limit,
  ]);
  drupal_exit();
}

/**
 * COUNT(*) only — no LIMIT, no fetchAll.
 */
function _modularform_dashboard_count(array $conditions, $user) {
  $is_admin = _modularform_is_site_admin();

  $query = db_select('modularform_forms', 'f')
    ->isNull('f.deleted_at');
  $query->addExpression('COUNT(DISTINCT f.id)', 'total');

  if (!$is_admin) {
    $query->leftJoin(
      'modularform_form_members', 'fm',
      'fm.form_id = f.id AND fm.uid = :uid AND fm.roles = :role',
      [':uid' => $user->uid, ':role' => 0]
    );
    $or = db_or();
    $or->condition('f.created_by', $user->uid);
    $or->isNotNull('fm.form_id');
    $query->condition($or);
  }

  if (!empty($conditions['search'])) {
    $like = '%' . db_like($conditions['search']) . '%';
    $or = db_or();
    $or->condition('f.title', $like, 'LIKE');
    $or->condition('f.description', $like, 'LIKE');
    $query->condition($or);
  }
  if (isset($conditions['status'])) {
    $query->condition('f.status', (int) $conditions['status']);
  }
  if (isset($conditions['visibility'])) {
    $query->condition('f.visibility', (int) $conditions['visibility']);
  }
  if (isset($conditions['created_by'])) {
    $query->condition('f.created_by', (int) $conditions['created_by']);
  }

  return (int) $query->execute()->fetchField();
}

/* ──────────────────────────────────────────────────────────
   DATA HELPERS
   ────────────────────────────────────────────────────────── */

/**
 * Fetch forms: tries Solr first, falls back to SQL.
 *
 * @return array  stdClass rows: id, title, status, created_by, created_at
 */
function _modularform_dashboard_fetch(array $conditions, $user, $sort = 'created_desc', $page = 0, $limit = 25) {
  if (module_exists('modularform_search')) {
    module_load_include('inc', 'modularform_search', 'includes/modularform_forms_search');
    $result = modularform_search_query_forms($conditions);
    if ($result !== NULL && !isset($result['error'])) {
      $sorted = _modularform_apply_sort($result, $sort);
      // slice for pagination since Solr returned everything
      return array_slice($sorted, $page * $limit, $limit);
    }
  }
  return _modularform_dashboard_sql($conditions, $user, $sort, $limit, $page);
}

/**
 * In-memory sort for Solr result arrays.
 */
function _modularform_apply_sort(array $forms, $sort) {
  usort($forms, function($a, $b) use ($sort) {
    switch ($sort) {
      case 'title_asc':
        return strcmp($a->title, $b->title);
      case 'title_desc':
        return strcmp($b->title, $a->title);
      case 'created_asc':
        return $a->created_at <=> $b->created_at;
      default:
        return $b->created_at <=> $a->created_at;
    }
  });
  return $forms;
}

function _modularform_dashboard_sql(array $conditions, $user, $sort, $limit = 25, $page = 0) {
  $is_admin = _modularform_is_site_admin();

  $query = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title', 'status', 'visibility', 'created_by', 'created_at'])
    ->isNull('f.deleted_at');

  if ($is_admin) {
    $query->addExpression(
      '(SELECT COUNT(*) FROM {modularform_responses} r WHERE r.form_id = f.id)',
      'response_count'
    );
  }
  else {
    $query->leftJoin(
      'modularform_form_members', 'fm',
      'fm.form_id = f.id AND fm.uid = :uid',
      [':uid' => $user->uid]
    );

    $query->addExpression(
      '(SELECT COUNT(*) FROM {modularform_responses} r WHERE r.form_id = f.id AND (f.created_by = :count_uid OR fm.roles = :creator))',
      'response_count',
      [':count_uid' => $user->uid, ':creator' => 0]
    );

    $or = db_or();
    $or->condition('f.created_by', $user->uid);
    $or->isNotNull('fm.form_id');
    $query->condition($or);
  }

  if (!empty($conditions['search'])) {
    $like = '%' . db_like($conditions['search']) . '%';
    $or = db_or();
    $or->condition('f.title', $like, 'LIKE');
    $or->condition('f.description', $like, 'LIKE');
    $query->condition($or);
  }

  if (isset($conditions['status'])) {
    $query->condition('f.status', (int) $conditions['status']);
  }

  if (isset($conditions['visibility'])) {
    $query->condition('f.visibility', (int) $conditions['visibility']);
  }

  if (isset($conditions['created_by'])) {
    $query->condition('f.created_by', (int) $conditions['created_by']);
  }

  switch ($sort) {
    case 'title_asc':   $query->orderBy('f.title', 'ASC');       break;
    case 'title_desc':  $query->orderBy('f.title', 'DESC');      break;
    case 'created_asc': $query->orderBy('f.created_at', 'ASC');  break;
    default:            $query->orderBy('f.created_at', 'DESC'); break;
  }

  $query->range($page * $limit, $limit);
  return $query->execute()->fetchAll();
}
