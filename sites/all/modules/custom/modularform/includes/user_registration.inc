<?php

/**
 * Implements hook_user_insert().
 * Fires after a new user account is saved.
 */

function modularform_invite_register_page($token) {
  // Validate token before showing anything
  $invite = db_select('modularform_form_invites', 'i')
    ->fields('i')
    ->condition('i.token', $token)
    ->condition('i.status', 0)   // 0 = pending/valid, not yet consumed
    ->execute()
    ->fetchObject();

  if (!$invite) {
    drupal_set_message(t('This invitation is invalid or has already been used.'), 'error');
    drupal_goto('<front>');
  }

  // Stash the invite in the session so post-registration can pick it up
  $_SESSION['pending_invite_token'] = $token;

  // Just render Drupal's normal registration form
  return drupal_get_form('user_register_form');
}

function modularform_user_insert(&$edit, $account, $category) {
  if (empty($_SESSION['pending_invite_token'])) {
    return;
  }

  $token = $_SESSION['pending_invite_token'];
  unset($_SESSION['pending_invite_token']);

  $invite = db_select('modularform_form_invites', 'i')
    ->fields('i', ['id', 'form_id'])
    ->condition('i.token', $token)
    ->condition('i.status', 0)
    ->execute()
    ->fetchObject();

  if (!$invite) {
    return;
  }

  $now = REQUEST_TIME;

  // Assign role directly — safe here because user insert is already done
  $role = user_role_load_by_name('temporary user');
  if (!$role) {
    watchdog('modularform', 'Role "temporary user" not found during invite acceptance for invite !id.', [
      '!id' => $invite->id,
    ], WATCHDOG_ERROR);
    throw new Exception('Required role "temporary user" not found.');
  }
  $txn = db_transaction();
  try {
    if ($role) {
      db_insert('users_roles')
        ->fields([
          'uid' => $account->uid,
          'rid' => $role->rid,
        ])
        ->execute();
    }

    db_insert('modularform_form_members')
      ->fields([
        'form_id' => $invite->form_id,
        'uid' => $account->uid,
        'roles' => 2,
        'status' => 0,
        'token' => $token,
        'notified' => 1,
        'created_at' => $now,
        'updated_at' => $now,
      ])
      ->execute();

    db_update('modularform_form_invites')
      ->fields(['status' => 1, 'updated_at' => $now])
      ->condition('id', $invite->id)
      ->execute();

    $_SESSION['pending_invite_form_id'] = $invite->form_id;
    $_SESSION['pending_invite_token_used'] = $token;
  }
  catch (Exception $e) {
    $txn->rollback();
    watchdog('modularform', 'Invite acceptance failed for invite !id: !msg', [
      '!id' => $invite->id,
      '!msg' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    // re-throw or return an error to the caller as appropriate
    throw $e;
  }
}

function modularform_user_presave(&$edit, $account, $category) {
  // Only activate if this is a new account coming via an invite
  if (!empty($_SESSION['pending_invite_token']) && empty($account->uid)) {
    $edit['status'] = 1;
    $account->status = 1;
  }
}

function modularform_user_login(&$edit, $account) {
  if (empty($_SESSION['pending_invite_form_id'])) {
    return;
  }

  $form_id = $_SESSION['pending_invite_form_id'];
  $token = $_SESSION['pending_invite_token_used'];  // need to stash this too
  unset($_SESSION['pending_invite_form_id']);
  unset($_SESSION['pending_invite_token_used']);

  drupal_goto('forms/' . $form_id . '/fill', ['query' => ['token' => $token]]);
}
