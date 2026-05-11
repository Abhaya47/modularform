<?php

function modularform_response_list_page() {
  // Fetch all forms keyed by id for lookup
  $forms = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title', 'editable', 'anonymous_responses'])
    ->execute()
    ->fetchAllAssoc('id');

  if (empty($forms)) {
    return '<p>' . t('No forms found.') . '</p>';
  }

  // Determine if any form is editable (to decide whether to show Updated col)
  $any_editable = FALSE;
  foreach ($forms as $f) {
    if ($f->editable) {
      $any_editable = TRUE;
      break;
    }
  }

  $header = [
    t('Form'),
    t('Email'),
    t('Submitted'),
  ];
  if ($any_editable) {
    $header[] = t('Updated');
  }
  $header[] = t('Operations');

  $query = db_select('modularform_responses', 'r')
    ->fields('r', ['id', 'form_id', 'email', 'submitted_at', 'updated_at'])
    ->orderBy('r.submitted_at', 'DESC');

  $query = $query->extend('PagerDefault')->limit(25);

  $responses = $query->execute()->fetchAll();

  $rows = [];
  foreach ($responses as $response) {
    $form = isset($forms[$response->form_id]) ? $forms[$response->form_id] : NULL;
    $form_title = $form ? check_plain($form->title) : t('Unknown form');

    $email = ($form && $form->anonymous_responses)
      ? t('Anonymous')
      : check_plain($response->email ?: t('N/A'));

    $submitted = format_date($response->submitted_at, 'custom', 'M j Y, g:i a');

    $row = [$form_title, $email, $submitted];

    if ($any_editable) {
      $row[] = ($form && $form->editable && $response->updated_at)
        ? format_date($response->updated_at, 'custom', 'M j Y, g:i a')
        : t('—');
    }

    $links = [
      'view' => [
        'title' => t('View'),
        'href'  => 'forms/response/' . $response->id,
      ],
      'delete' => [
        'title'      => t('Delete'),
        'href'       => 'forms/response/delete/' . $response->id,
      ],
    ];

    $row[] = $links;
    $rows[] = $row;
  }

  return [
    '#theme'    => 'response_list',
    '#header'   => $header,
    '#rows'     => $rows,
    '#attached' => [
      'css' => [drupal_get_path('module', 'modularform') . '/css/dashboard.css'],
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
