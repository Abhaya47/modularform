<?php

function form_deleted_dashboard_page() {
  $header = [
    t('Name'),
    t('Owner'),
    t('Status'),
    t('Created'),
    t('Deleted On'),
    t('Operations'),
  ];
  $rows = [];

  // Get form IDs where user is a creator member (role 0).
  if (_modularform_is_site_admin()) {
    $query = db_select('modularform_forms', 'f');
    $query->fields('f', [
      'id',
      'title',
      'status',
      'created_by',
      'created_at',
      'deleted_at',
    ]);
    $query->isNotNull('f.deleted_at');
    $query->orderBy('f.deleted_at', 'DESC');
    $forms = $query->execute()->fetchAll();
  }
  else {
    global $user;
    $query = db_select('modularform_forms', 'f');
    $query->fields('f', [
      'id',
      'title',
      'status',
      'created_by',
      'created_at',
      'deleted_at',
    ]);
    $query->leftJoin('modularform_form_members', 'fm', 'fm.form_id = f.id AND fm.uid = :uid AND fm.roles = :role', [
      ':uid' => $user->uid,
      ':role' => 0,
    ]);
    $query->isNotNull('f.deleted_at');
    $query->orderBy('f.deleted_at', 'DESC');
    $or = db_or();
    $or->condition('f.created_by', $user->uid);
    $or->isNotNull('fm.form_id');
    $query->condition($or);
    $forms = $query->execute()->fetchAll();
  }

  if (empty($forms)) {
    return '<p>' . t('No deleted forms found.') . '</p>';
  }

  $uids = array_unique(array_column((array) $forms, 'created_by'));
  $users = user_load_multiple($uids);

  foreach ($forms as $form) {
    $owner = isset($users[$form->created_by]) ? format_username($users[$form->created_by]) : t('Unknown');
    $status = $form->status ? t('Published') : t('Draft');
    $created = format_date($form->created_at, 'custom', 'M j Y');
    $deleted_on = format_date($form->deleted_at, 'custom', 'M j Y');

    $links = [
      'restore' => [
        'title' => t('Restore'),
        'href'  => 'forms/restore/' . $form->id,
        'query' => ['token' => drupal_get_token('modularform_restore_' . $form->id)],
      ],
      'permanent_delete' => [
        'title' => t('Delete Permanently'),
        'href'  => 'forms/permanent_delete/' . $form->id,
        'query' => ['token' => drupal_get_token('modularform_delete_' . $form->id)],
      ],
    ];

    $rows[] = [
      check_plain($form->title),
      check_plain($owner),
      $status,
      $created,
      $deleted_on,
      $links,
    ];
  }

  return [
    '#theme' => 'trash',
    '#header' => $header,
    '#rows' => $rows,
    '#attached' => [
      'css' => [drupal_get_path('module', 'modularform') . '/css/dashboard.css'],
    ],
  ];
}

/**
 * Permanently delete a form and all its associated data from the DB.
 */
function modularform_perma_delete($form_id) {
  if (!drupal_valid_token($_GET['token'] ?? '', 'modularform_delete_' . $form_id)) {
    return drupal_access_denied();
  }

  $form = db_select('modularform_forms', 'f')
    ->fields('f')
    ->condition('id', $form_id)
    ->isNotNull('deleted_at')
    ->execute()
    ->fetchObject();
  if (!$form) {
    drupal_set_message(t('Form not found or has not been deleted first.'), 'error');
    drupal_goto('forms/trash');
  }
  $txn = db_transaction();
  try {
    $response_ids = db_select('modularform_responses', 'r')
      ->fields('r', ['id'])
      ->condition('r.form_id', $form_id)
      ->execute()
      ->fetchCol();
    foreach ($response_ids as $response_id) {
      _modularform_delete_response_data($response_id);
    }
    db_delete('modularform_question_options')
      ->condition('form_id', $form_id)
      ->execute();
    db_delete('modularform_validation_rules')
      ->condition('form_id', $form_id)
      ->execute();
    db_delete('modularform_questions')
      ->condition('form_id', $form_id)
      ->execute();
    db_delete('modularform_sections')
      ->condition('form_id', $form_id)
      ->execute();
    entity_delete('modularform_form', $form_id);
    drupal_set_message(t('Form "@title" has been permanently deleted.', ['@title' => $form->title]));
  }
  catch (Exception $e) {
    $txn->rollback();
    watchdog('modularform', 'Failed to permanently delete form @id: @msg', [
      '@id' => $form_id,
      '@msg' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    drupal_set_message(t('Failed to permanently delete the form. Please try again.'), 'error');
  }
  drupal_goto('forms/manage');
}

/**
 * Restore a form
 */
function modularform_restore($form_id) {
  if (!drupal_valid_token($_GET['token'] ?? '', 'modularform_restore_' . $form_id)) {
    return drupal_access_denied();
  }
  $form = db_select('modularform_forms', 'f')
    ->fields('f')
    ->condition('id', $form_id)
    ->isNotNull('deleted_at')
    ->execute()
    ->fetchObject();

  if (!$form) {
    drupal_set_message(t('Form not found or is not deleted.'), 'error');
    drupal_goto('forms/trash');
  }

  db_update('modularform_forms')
    ->fields(['deleted_at' => NULL])
    ->condition('id', $form_id)
    ->execute();

  drupal_set_message(t('Form "@title" has been restored.', ['@title' => $form->title]));
  drupal_goto('forms/manage');
}
