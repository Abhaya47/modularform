<?php

module_load_include('inc', 'modularform', 'includes/searchbar_form');
function form_dashboard_page() {

  // ── Search bar ─────────────────────────────────────────────────────────────
  $current_search = isset($_GET['search']) ? check_plain(trim($_GET['search'])) : '';

  // ── Table (your existing code, untouched) ──────────────────────────────────
  $header = [
    t('Name'), t('Owner'), t('Status'), t('Created'), t('Operations'),
  ];
  $rows = [];
  if($current_search){
    if (module_exists('modularform_search')) {
      // TODO: Solr search
      module_load_include('inc', 'modularform_search', 'includes/modularform_search.solr');
      $forms = modularform_search_solr_title($current_search);
    }
    else {
      // Fallback: plain SQL LIKE on title
      $query = db_select('modularform_forms', 'f')
        ->fields('f', ['id', 'title', 'status', 'created_by', 'created_at'])
        ->isNull('f.deleted_at')
        ->orderBy('f.created_at', 'DESC');

      if (!empty($current_search)) {
        $query->condition('f.title', '%' . db_like($current_search) . '%', 'LIKE');
      }

      $forms = $query->execute()->fetchAll();
    }
  }
  else{
    $forms = db_select('modularform_forms', 'f')
      ->fields('f', ['id', 'title', 'status', 'created_by', 'created_at'])
      ->isNull('f.deleted_at')
      ->orderBy('f.created_at', 'DESC')
      ->execute()
      ->fetchAll();
  }


  $uids  = array_unique(array_column((array) $forms, 'created_by'));
  $users = user_load_multiple($uids);

  foreach ($forms as $form) {
    $owner   = isset($users[$form->created_by]) ? format_username($users[$form->created_by]) : t('Unknown');
    $status  = $form->status ? t('Published') : t('Draft');
    $created = format_date($form->created_at, 'custom', 'M j Y');
    $links   = [
      'builder_preview' => ['title' => t('Preview Survey'), 'href' => 'forms/' . $form->id . '/builder/preview'],
      'builder_edit'    => ['title' => t('Edit Survey'),    'href' => 'forms/' . $form->id . '/builder/'],
      'settings_edit'   => ['title' => t('Edit Settings'),  'href' => 'forms/edit/' . $form->id],
      'delete'          => ['title' => t('Delete Survey'),  'href' => 'forms/delete/' . $form->id],
    ];
    $rows[] = [check_plain($form->title), check_plain($owner), $status, $created, $links];
  }

  $build['table'] = [
    '#theme'  => 'dashboard',
    '#header' => $header,
    '#rows'   => $rows,
    '#search_bar' => drupal_get_form('modularform_search_bar_form', $current_search),
  ];

  $build['#attached']['css'][] = drupal_get_path('module', 'modularform') . '/css/dashboard.css';

  return $build;
}

