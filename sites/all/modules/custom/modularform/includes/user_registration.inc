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
    ->condition('i.status', 1)
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
    ->condition('i.status', 1)
    ->execute()
    ->fetchObject();

  if (!$invite) {
    return;
  }

  $now = REQUEST_TIME;

  // Assign temporary user role
  $role = user_role_load_by_name('temporary user');
  if ($role) {
    db_merge('users_roles')
      ->key(['uid' => $account->uid, 'rid' => $role->rid])
      ->fields(['uid' => $account->uid, 'rid' => $role->rid])
      ->execute();
  }

  db_insert('modularform_form_members')
    ->fields([
      'form_id'    => $invite->form_id,
      'uid'        => $account->uid,
      'roles'      => 2,
      'status'     => 0,
      'token'      => $token,   // reuse the invite token
      'notified'   => 1,        // they already got the email
      'created_at' => $now,
      'updated_at' => $now,
    ])
    ->execute();

  db_update('modularform_form_invites')
    ->fields([
      'status'     => 1,         // no longer pending
      'uid'        => $account->uid,
      'updated_at' => $now,
    ])
    ->condition('id', $invite->id)
    ->execute();
}
