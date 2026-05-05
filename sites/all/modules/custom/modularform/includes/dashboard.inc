<?php

function form_dashboard_page() {
  $header = [
    t('Name'),
    t('Owner'),
    t('Status'),
    t('Created'),
    t('Operations'),
  ];

  $rows = [];

  $forms = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title', 'status', 'created_by', 'created_at'])
    ->isNull('deleted_at')
    ->orderBy('created_at', 'DESC')
    ->execute()
    ->fetchAll();

  // Batch load usernames
  $uids = array_unique(array_column((array) $forms, 'created_by'));
  $users = user_load_multiple($uids);

  foreach ($forms as $form) {
    $owner = isset($users[$form->created_by]) ? format_username($users[$form->created_by]) : t('Unknown');
    $status = $form->status ? t('Published') : t('Draft');
    $created = format_date($form->created_at, 'custom', 'M j Y');
    $links = [
      'builder_preview' => [
        'title' => t('Preview Survey'),
        'href' => 'forms/' . $form->id . '/builder/preview',
      ],
      'builder_edit' => [
        'title' => t('Edit Survey'),
        'href' => 'forms/' . $form->id . '/builder/',
      ],
      'settings_edit' => [
        'title' => t('Edit Settings'),
        'href' => 'forms/edit/' . $form->id,
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
      $links,
    ];
  }

  return [
    '#theme' => 'dashboard',
    '#header' => $header,
    '#rows' => $rows,
    '#attached' => [
      'css' => [drupal_get_path('module', 'modularform') . '/css/dashboard.css'],
    ],
  ];
}
