<?php

#TODO role based member addition
function modularform_create_form($form, &$form_state, $id = NULL) {
  // Load existing form entity if editing.
  $modular_form = NULL;
  if ($id) {
    $results = entity_get_controller('modularform_form')->load([$id]);
    $modular_form = reset($results) ?: NULL;
  }

  $val = function($key, $default = NULL) use (&$form_state, $modular_form) {
    if (isset($form_state['values'][$key])) {
      return $form_state['values'][$key];
    }
    if ($modular_form && isset($modular_form->$key)) {
      return $modular_form->$key;
    }
    return $default;
  };

  $form['#tree'] = TRUE;

  $form['id'] = [
    '#type' => 'hidden',
    '#default_value' => $modular_form ? $modular_form->id : '',
  ];

  $form['title'] = [
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#required' => TRUE,
    '#default_value' => $val('title', t('Untitled form')),
  ];

  $form['description'] = [
    '#type' => 'textfield',
    '#title' => t('Description'),
    '#default_value' => $val('description', ''),
  ];

  $form['status'] = [
    '#type' => 'radios',
    '#title' => t('Status'),
    '#options' => ['0' => t('Draft'), '1' => t('Published')],
    '#required' => TRUE,
    '#default_value' => $val('status', 0),
  ];

  // ── Visibility ────────────────────────────────────────────────────────────

  $form['visibility'] = [
    '#type' => 'select',
    '#title' => t('Visibility'),
    '#options' => [
      'public' => t('Public'),
      'restricted' => t('Restricted (by role)'),
      'private' => t('Private (specific people)'),
    ],
    '#required' => TRUE,
    '#default_value' => $val('visibility', 'public'),
    '#ajax' => [
      'callback' => 'modularform_visibility_ajax',
      'wrapper' => 'visibility-options-wrapper',
    ],
  ];

  $form['visibility_options_wrapper'] = [
    '#type' => 'container',
    '#attributes' => ['id' => 'visibility-options-wrapper'],
  ];

  $visibility = $val('visibility', 'public');

  if ($visibility === 'restricted') {
    $roles = array_diff_key(user_roles(TRUE), [
      DRUPAL_AUTHENTICATED_RID => TRUE,
    ]);
    $form['visibility_options_wrapper']['roles'] = [
      '#type' => 'checkboxes',
      '#title' => t('Allowed roles'),
      '#options' => $roles,
      '#required' => TRUE,
      '#default_value' => $val('roles', []),
      '#description' => t('Members of your organization with these roles can access this form.'),
    ];
  }

  if ($visibility === 'private') {
    $form['visibility_options_wrapper']['people_wrapper'] = [
      '#type' => 'container',
      '#tree' => TRUE,
      '#attributes' => ['class' => ['modularform-people-wrapper']],
    ];

    $form['visibility_options_wrapper']['people_wrapper']['label'] = [
      '#markup' => '<label>' . t('Add people') . '</label>' .
        '<p class="description">' .
        t('Search by username or email. Unknown emails will receive a registration invitation.') .
        '</p>',
    ];

    // Visible anchor input — Select2 attaches to this element
    $form['visibility_options_wrapper']['people_wrapper']['people_select'] = [
      '#type' => 'textfield',
      '#title' => '',
      '#attributes' => [
        'id' => 'modularform-people-select',
        'class' => ['modularform-people-select'],
        'data-ac-url' => url('modularform/user-autocomplete'),
        'autocomplete' => 'on',
      ],
    ];

    // Hidden field that actually holds the value Drupal submits
    $form['visibility_options_wrapper']['people_wrapper']['people_value'] = [
      '#type' => 'hidden',
      '#default_value' => '',
      '#attributes' => [
        'id' => 'modularform-people-input',
        'class' => ['modularform-people-hidden'],
      ],
    ];

    $form['visibility_options_wrapper']['people_wrapper']['#attached']['js'][] = [
      'data' => drupal_get_path('module', 'modularform') . '/js/modularform.people.js',
      'type' => 'file',
    ];
  }

  // ── Scheduled ─────────────────────────────────────────────────────────────

  $form['scheduled'] = [
    '#type' => 'checkbox',
    '#title' => t('Schedule a publishing date?'),
    '#default_value' => $val('scheduled', 0),
    '#ajax' => [
      'callback' => 'modularform_scheduled_ajax',
      'wrapper' => 'scheduled-date-wrapper',
    ],
    '#weight' => 9,
  ];

  $form['scheduled_date_wrapper'] = [
    '#type' => 'container',
    '#attributes' => ['id' => 'scheduled-date-wrapper'],
    '#weight' => 10,
  ];

  if ($val('scheduled', 0)) {
    $timestamp = ($modular_form && $modular_form->scheduled_date)
      ? (int) $modular_form->scheduled_date
      : REQUEST_TIME;

    $form['scheduled_date_wrapper']['scheduled_date'] = [
      '#type' => 'date',
      '#title' => t('Publish on'),
      '#required' => TRUE,
      '#default_value' => [
        'year' => date('Y', $timestamp),
        'month' => date('n', $timestamp),
        'day' => date('j', $timestamp),
      ],
    ];
  }

  // ── Closes ────────────────────────────────────────────────────────────────

  $form['closes'] = [
    '#type' => 'checkbox',
    '#title' => t('Set a closing date?'),
    '#default_value' => $val('closes', 0),
    '#ajax' => [
      'callback' => 'modularform_closes_ajax',
      'wrapper' => 'closes-at-wrapper',
    ],
    '#weight' => 11,
  ];

  $form['closes_at_wrapper'] = [
    '#type' => 'container',
    '#attributes' => ['id' => 'closes-at-wrapper'],
    '#weight' => 12,
  ];

  if ($val('closes', 0)) {
    $timestamp = ($modular_form && $modular_form->closes_at)
      ? (int) $modular_form->closes_at
      : REQUEST_TIME;

    $form['closes_at_wrapper']['closes_at'] = [
      '#type' => 'date',
      '#title' => t('Closes at'),
      '#required' => TRUE,
      '#default_value' => [
        'year' => date('Y', $timestamp),
        'month' => date('n', $timestamp),
        'day' => date('j', $timestamp),
      ],
    ];
  }

  // ── Submit ────────────────────────────────────────────────────────────────

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Start Building'),
    '#weight' => 200,
  ];

  $form['#theme'] = ['modularform_initial_template'];
  $form['#attached']['css'][] = drupal_get_path('module', 'modularform') . '/css/modularform-initial.css';

  return $form;
}


function your_form_add_more_callback($form, &$form_state) {
  return $form['visibility_options_wrapper']['people'];
}

// ── AJAX callbacks ────────────────────────────────────────────────────────────

function modularform_scheduled_ajax($form, &$form_state) {
  return $form['scheduled_date_wrapper'];
}

function modularform_closes_ajax($form, &$form_state) {
  return $form['closes_at_wrapper'];
}

function modularform_visibility_ajax($form, &$form_state) {
  return $form['visibility_options_wrapper'];
}

// ── Submit ────────────────────────────────────────────────────────────────────

function modularform_create_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $id = $values['id'];
  $controller = entity_get_controller('modularform_form');
  $now = (new DateTime('now', new DateTimeZone('UTC')))->getTimestamp();

  if (!empty($id)) {
    $results = $controller->load([$id]);
    $entity = reset($results);
    $is_editing = TRUE;
  }
  else {
    $entity = $controller->create(['type' => 'modularform_form']);
    $entity->slug = modularform_generate_random_slug($values['title']);
    $entity->created_at = $now;
    global $user;
    $entity->created_by = $user->uid;
  }

  $entity->title = $values['title'];
  $entity->description = $values['description'];
  $entity->status = $values['status'];
  $entity->updated_at = $now;

  // ── scheduled_date ────────────────────────────────────────────────────────
  $scheduled_date = 0;
  if (!empty($values['scheduled']) && !empty($values['scheduled_date_wrapper']['scheduled_date'])) {
    $d = $values['scheduled_date_wrapper']['scheduled_date'];
    $scheduled_date = mktime(0, 0, 0, $d['month'], $d['day'], $d['year']);
  }
  $entity->scheduled = !empty($values['scheduled']) ? 1 : 0;
  $entity->scheduled_date = $scheduled_date;

  // ── closes_at ─────────────────────────────────────────────────────────────
  $closes_at = 0;
  if (!empty($values['closes']) && !empty($values['closes_at_wrapper']['closes_at'])) {
    $d = $values['closes_at_wrapper']['closes_at'];
    $closes_at = mktime(0, 0, 0, $d['month'], $d['day'], $d['year']);
  }
  $entity->closes = !empty($values['closes']) ? 1 : 0;
  $entity->closes_at = $closes_at;


  // ── visibility ────────────────────────────────────────────────────────────
  $visibility = $values['visibility'];
  $form_id = $entity->id;

  if ($visibility === 'restricted') {
    $selected_rids = array_filter($values['visibility_options_wrapper']['roles']);

    $uids_to_add = [];
    if (isset($is_editing)) {
      // Edit: sync existing rows, get back only the new uids.
      $uids_to_add = _modularform_restricted_people_edit($form_id, $selected_rids, $entity, $now);
    }

    // Create: $uids_to_add is empty, add function resolves the full list.
    // Edit:   $uids_to_add is the diff, add function inserts only those.
    _modularform_restricted_people_add($form_id, $selected_rids, $entity, $now, $uids_to_add);
  }

  elseif ($visibility === 'private') {
    $raw = isset($values['visibility_options_wrapper']['people_wrapper']['people_value'])
      ? $values['visibility_options_wrapper']['people_wrapper']['people_value']
      : '';

    if (isset($is_editing)) {
      $live_people = _modularform_edit_private_people($form_id, $raw, $entity, $now);
    }
    _modularform_save_private_people($form_id, $raw, $entity, $now);
  }

  $entity->save();

  $message = !empty($id) ? 'Form %title updated.' : 'Form %title saved in draft.';
  drupal_set_message(t($message, ['%title' => $entity->title]));
  drupal_goto('forms/' . $entity->id . '/builder');
}

/**
 * Generates a unique, URL-friendly slug from a title.
 */
function modularform_generate_random_slug($title) {
  $slug = preg_replace('/[^a-z0-9]+/', '_', strtolower($title));
  $slug = trim($slug, '_');

  $random_suffix = substr(md5(uniqid(mt_rand(), TRUE)), 0, 4);
  $final_slug = $slug . '_' . $random_suffix;

  if (_modularform_slug_exists($final_slug)) {
    return modularform_generate_random_slug($title);
  }

  return $final_slug;
}

/**
 * Internal helper to check the database for an existing slug.
 */
function _modularform_slug_exists($slug) {
  return db_query("SELECT 1 FROM {modularform_forms} WHERE slug = :slug", [':slug' => $slug])->fetchField();
}

function modularform_soft_delete($form_id) {
  $form = db_select('modularform_forms', 'mf')
    ->fields('mf')
    ->condition('id', $form_id)
    ->execute()
    ->fetchObject();

  if (!$form) {
    drupal_set_message(t('Form not found.'), 'error');
    drupal_goto('forms/manage');
  }

  $updated = db_update('modularform_forms')
    ->fields(['deleted_at' => REQUEST_TIME])
    ->condition('id', $form_id)
    ->execute();

  if ($updated) {
    drupal_set_message(t('Form "@title" has been deleted.', ['@title' => $form->title]));
  }
  else {
    drupal_set_message(t('Failed to delete the form.'), 'error');
  }

  drupal_goto('forms/manage');
}

// ── Autocomplete endpoint ─────────────────────────────────────────────────────
// Path: modularform/user-autocomplete?q=<search>
// Returns Select2 3.5.4 format: { results: [{id, text, uid, email, exists}] }

function modularform_user_autocomplete() {
  $search = isset($_GET['q']) ? trim(check_plain($_GET['q'])) : '';

  $results = [];

  if (drupal_strlen($search) >= 1) {
    // Search by name OR email in {users} table (skip uid=0 anonymous)
    $query = db_select('users', 'u')
      ->fields('u', ['uid', 'name', 'mail'])
      ->condition('u.uid', 0, '>')
      ->condition('u.status', 1)           // active users only
      ->groupBy('u.uid')
      ->range(0, 20);

    $or = db_or()
      ->condition('u.name', '%' . db_like($search) . '%', 'LIKE')
      ->condition('u.mail', '%' . db_like($search) . '%', 'LIKE');
    $query->condition($or);

    $rows = $query->execute();
    foreach ($rows as $row) {
      $results[] = [
        'id' => 'uid:' . $row->uid,
        // prefixed so JS knows it's a real user
        'text' => $row->name . ' (' . $row->mail . ')',
        'uid' => $row->uid,
        'email' => $row->mail,
        'exists' => TRUE,
      ];
    }
  }

  drupal_json_output(['results' => $results, 'more' => FALSE]);
  drupal_exit();
}


// ── Form field builder (called from modularform_create_form) ──────────────────
// Drop this block in place of the old $form['visibility_options_wrapper']['people']
// block inside the if ($visibility === 'private') branch.
function _modularform_people_field(&$form, &$form_state) {
  $acUrl = url('modularform/user-autocomplete');

  // Single hidden field carries a comma-separated list of
  //   uid:<uid>          — existing Drupal user
  //   email:<address>    — external invite
  // Example: "uid:3,uid:7,email:newperson@example.com"
  $field = [
    '#type' => 'hidden',
    '#default_value' => '',
    '#attributes' => [
      'id' => 'modularform-people-input',
      'class' => ['modularform-people-hidden'],
      'data-ac-url' => $acUrl,
    ],
  ];

  // Visible label + Select2 container rendered via #prefix on a sibling markup
  $wrapper = [
    '#type' => 'container',
    '#tree' => TRUE,
    '#attributes' => ['class' => ['modularform-people-wrapper']],
  ];

  $wrapper['label'] = [
    '#markup' => '<label>' . t('Add people') . '</label>
                  <p class="description">' .
      t('Search by username or email. Unknown emails will receive an invitation.') .
      '</p>',
  ];

  $wrapper['people_value'] = $field;

  // Attach our JS behaviour — Select2 is already loaded via hook_library
  $wrapper['#attached']['js'][] = [
    'data' => drupal_get_path('module', 'modularform') . '/js/modularform.people.js',
    'type' => 'file',
  ];

  return $wrapper;
}

/**
 * Returns an array of uids for all users that have any of the given role IDs.
 *
 * @param array $rids
 *   Array of Drupal role IDs (rids) to look up.
 *
 * @return array
 *   Flat array of uids.
 */
function _modularform_get_protected_people(array $rids) {
  if (empty($rids)) {
    return [];
  }

  $uids = db_select('users_roles', 'ur')
    ->fields('ur', ['uid'])
    ->condition('ur.rid', $rids, 'IN')
    ->execute()
    ->fetchCol();

  return array_unique(array_map('intval', $uids));
}


/**
 * Syncs role-based members for an existing form.
 *
 * - Removes members whose uid is no longer in the new role set.
 * - Updates updated_at for members who remain.
 * - Returns the uids that still need to be inserted (new additions).
 *
 * @param int   $form_id
 * @param array $selected_rids
 * @param object $entity
 * @param int   $now
 *
 * @return array
 *   Uids to be inserted by _modularform_restricted_people_add().
 */
function _modularform_restricted_people_edit($form_id, array $selected_rids, $entity, $now) {
  $new_uids = _modularform_get_protected_people($selected_rids);

  // Fetch existing role-synced viewer rows (uid > 0, roles = 1).
  $existing = db_select('modularform_form_members', 'm')
    ->fields('m', ['id', 'uid'])
    ->condition('m.form_id', $form_id)
    ->condition('m.roles', 2)        // viewer
    ->condition('m.uid', 0, '>')     // real Drupal users only
    ->execute()
    ->fetchAllAssoc('uid');

  $existing_uids = array_map('intval', array_keys($existing));

  $to_remove = array_diff($existing_uids, $new_uids);
  $to_keep   = array_intersect($existing_uids, $new_uids);
  $to_add    = array_diff($new_uids, $existing_uids);

  // Remove members no longer in the role set.
  if (!empty($to_remove)) {
    db_delete('modularform_form_members')
      ->condition('form_id', $form_id)
      ->condition('uid', $to_remove, 'IN')
      ->condition('roles', 2)
      ->execute();
  }

  // Bump updated_at for members that are staying.
  if (!empty($to_keep)) {
    db_update('modularform_form_members')
      ->fields(['updated_at' => $now])
      ->condition('form_id', $form_id)
      ->condition('uid', $to_keep, 'IN')
      ->condition('roles', 1)
      ->execute();
  }

  // Hand off new uids to the add function.
  return array_values($to_add);
}

/**
 * Inserts member rows for uids that are not yet in the form's member table.
 *
 * On a create flow, pass the full uid list.
 * On an edit flow, pass only the diff returned by _modularform_restricted_people_edit().
 *
 * @param int    $form_id
 * @param array  $selected_rids  (unused here, kept for signature consistency)
 * @param object $entity
 * @param int    $now
 * @param array  $uids_to_add    Explicit uid list; if empty the function
 *                               resolves it from $selected_rids itself (create path).
 */
function _modularform_restricted_people_add($form_id, array $selected_rids, $entity, $now, array $uids_to_add = []) {
  if (empty($uids_to_add)) {
    return;
  }
  $uids_to_add = _modularform_get_protected_people($selected_rids);

  $query = db_insert('modularform_form_members')
    ->fields(['form_id', 'uid', 'roles', 'status', 'token', 'created_at', 'updated_at']);

  $new_rows = [];
  foreach ($uids_to_add as $uid) {
    $token = hash('sha256', $form_id . '_' . $uid . '_' . drupal_random_key(16));
    $new_rows[] = ['uid' => (int) $uid, 'token' => $token];

    $query->values([
      'form_id'    => $form_id,
      'uid'        => (int) $uid,
      'roles'      => 2,
      'status'     => 0,
      'token'      => $token,
      'created_at' => $now,
      'updated_at' => $now,
    ]);
  }

  $query->execute();

  _modularform_queue_restricted_notifications($form_id, $entity, $new_rows);
}



/**
 * Parses raw people_value into drupal uids and external emails.
 *
 * @param string $raw
 *   Comma-separated string of uid:X and email:X entries.
 *
 * @return array
 *   ['drupal' => [int, ...], 'external' => [string, ...]]
 */
function _modularform_parse_private_people($raw) {
  $result = ['drupal' => [], 'external' => []];

  if (empty($raw)) {
    return $result;
  }

  foreach (array_filter(explode(',', $raw)) as $item) {
    $item = trim($item);
    if (empty($item)) {
      continue;
    }

    if (strpos($item, 'uid:') === 0) {
      $uid = (int) substr($item, 4);
      if ($uid > 0) {
        $result['drupal'][] = $uid;
      }
    }
    elseif (strpos($item, 'email:') === 0) {
      $email = trim(substr($item, 6));
      if (valid_email_address($email)) {
        $result['external'][] = $email;
      }
    }
  }

  return $result;
}


/**
 * Inserts member rows for private Drupal users not already in the table.
 *
 * @param int   $form_id
 * @param array $uids
 * @param int   $now
 *
 * @return array
 *   Uids that were actually inserted (not already existing).
 */
function _modularform_save_private_drupal($form_id, array $uids, $now) {
  if (empty($uids)) {
    return [];
  }

  // Find uids already in the table for this form.
  $existing_uids = db_select('modularform_form_members', 'm')
    ->fields('m', ['uid'])
    ->condition('m.form_id', $form_id)
    ->condition('m.uid', $uids, 'IN')
    ->execute()
    ->fetchCol();

  $to_insert = array_diff($uids, $existing_uids);

  if (empty($to_insert)) {
    return [];
  }

  $query = db_insert('modularform_form_members')
    ->fields(['form_id', 'uid', 'roles', 'status', 'token', 'notified', 'created_at', 'updated_at']);

  foreach ($to_insert as $uid) {
    $token = hash('sha256', $form_id . '_' . $uid . '_' . drupal_random_key(16));
    $query->values([
      'form_id'    => $form_id,
      'uid'        => (int) $uid,
      'roles'      => 2,   // participant
      'status'     => 0,
      'token'      => $token,
      'notified'   => 0,
      'created_at' => $now,
      'updated_at' => $now,
    ]);
  }

  $query->execute();

  return array_values($to_insert);
}


/**
 * Inserts invite rows for external emails not already invited.
 *
 * @param int   $form_id
 * @param array $emails
 * @param int   $now
 *
 * @return array
 *   Emails that were actually inserted (not already existing).
 */
function _modularform_save_private_external($form_id, array $emails, $now) {
  if (empty($emails)) {
    return [];
  }

  // Find emails already invited for this form.
  $existing_emails = db_select('modularform_form_invites', 'i')
    ->fields('i', ['email'])
    ->condition('i.form_id', $form_id)
    ->condition('i.email', $emails, 'IN')
    ->execute()
    ->fetchCol();

  $to_insert = array_diff($emails, $existing_emails);

  if (empty($to_insert)) {
    return [];
  }

  $query = db_insert('modularform_form_invites')
    ->fields(['form_id', 'email', 'token', 'status', 'notified', 'created_at']);

  foreach ($to_insert as $email) {
    $token = hash('sha256', $form_id . '_' . $email . '_' . drupal_random_key(16));
    $query->values([
      'form_id'    => $form_id,
      'email'      => $email,
      'token'      => $token,
      'status'     => 1,   // pending
      'notified'   => 0,
      'created_at' => $now,
    ]);
  }

  $query->execute();

  return array_values($to_insert);
}


/**
 * Queues notification emails for all unnotified private members and invites.
 *
 * @param int    $form_id
 * @param object $entity
 * @param int    $now
 */
function _modularform_queue_private_notifications($form_id, $entity, $now) {
  if ((int) $entity->status !== 1) {
    return;
  }

  $queue = DrupalQueue::get('modularform_mail_queue');

  // ── Drupal users ───────────────────────────────────────────────────────────
  $member_rows = db_select('modularform_form_members', 'm')
    ->fields('m', ['uid', 'token'])
    ->condition('m.form_id', $form_id)
    ->condition('m.roles', 2)
    ->condition('m.notified', 0)
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  if (!empty($member_rows)) {
    $accounts = user_load_multiple(array_column($member_rows, 'uid'));

    foreach ($member_rows as $row) {
      $account = isset($accounts[$row['uid']]) ? $accounts[$row['uid']] : NULL;
      if (!$account || empty($account->mail)) {
        continue;
      }

      $form_url = url('forms/' . $entity->slug . '/fill', [
        'absolute' => TRUE,
        'query'    => ['token' => $row['token']],
      ]);

      $queue->createItem([
        'key'    => 'form_notify_user',
        'to'     => $account->mail,
        'params' => [
          'form_id'  => $entity->title,
          'uid'      => $account->uid,
          'form_url' => $form_url,
        ],
      ]);
    }

    db_update('modularform_form_members')
      ->fields(['notified' => 1, 'updated_at' => $now])
      ->condition('form_id', $form_id)
      ->condition('uid', array_column($member_rows, 'uid'), 'IN')
      ->execute();
  }

  // ── External invites ───────────────────────────────────────────────────────
  $invite_rows = db_select('modularform_form_invites', 'i')
    ->fields('i', ['email', 'token'])
    ->condition('i.form_id', $form_id)
    ->condition('i.notified', 0)
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  if (!empty($invite_rows)) {
    foreach ($invite_rows as $row) {
      $queue->createItem([
        'key'    => 'form_invite_external',
        'to'     => $row['email'],
        'params' => [
          'form_id'      => $entity->id,
          'register_url' => url('register/invite/' . $row['token'], ['absolute' => TRUE]),
        ],
      ]);
    }

    db_update('modularform_form_invites')
      ->fields(['notified' => 1])
      ->condition('form_id', $form_id)
      ->condition('email', array_column($invite_rows, 'email'), 'IN')
      ->execute();
  }
}


/**
 * Saves private people on create or edit, and queues notifications if published.
 *
 * @param int    $form_id
 * @param string $raw
 * @param object $entity
 * @param int    $now
 */
function _modularform_save_private_people($form_id, $raw, $entity, $now) {
  $parsed = _modularform_parse_private_people($raw);
  _modularform_save_private_drupal($form_id, $parsed['drupal'], $now);
  _modularform_save_private_external($form_id, $parsed['external'], $now);
  _modularform_queue_private_notifications($form_id, $entity, $now);
}


/**
 * Syncs private people on edit — removes dropped members, adds new ones.
 *
 * @param int    $form_id
 * @param string $raw
 * @param object $entity
 * @param int    $now
 */
function _modularform_edit_private_people($form_id, $raw, $entity, $now) {
  $parsed = _modularform_parse_private_people($raw);

  // ── Drupal users ───────────────────────────────────────────────────────────
  $existing_uids = db_select('modularform_form_members', 'm')
    ->fields('m', ['uid'])
    ->condition('m.form_id', $form_id)
    ->condition('m.roles', 2)
    ->execute()
    ->fetchCol();

  $to_remove = array_diff($existing_uids, $parsed['drupal']);

  if (!empty($to_remove)) {
    db_delete('modularform_form_members')
      ->condition('form_id', $form_id)
      ->condition('uid', $to_remove, 'IN')
      ->condition('roles', 2)
      ->execute();
  }

  _modularform_save_private_drupal($form_id, $parsed['drupal'], $now);

  // ── External invites ───────────────────────────────────────────────────────
  $existing_emails = db_select('modularform_form_invites', 'i')
    ->fields('i', ['email'])
    ->condition('i.form_id', $form_id)
    ->execute()
    ->fetchCol();

  $to_remove_emails = array_diff($existing_emails, $parsed['external']);

  if (!empty($to_remove_emails)) {
    db_delete('modularform_form_invites')
      ->condition('form_id', $form_id)
      ->condition('email', $to_remove_emails, 'IN')
      ->execute();
  }

  _modularform_save_private_external($form_id, $parsed['external'], $now);
  _modularform_queue_private_notifications($form_id, $entity, $now);
}

/**
 * Queues notification emails for newly added restricted members.
 *
 * @param int    $form_id
 * @param object $entity
 * @param array  $new_rows
 *   Array of ['uid' => int, 'token' => string] for each inserted member.
 */
function _modularform_queue_restricted_notifications($form_id, $entity, array $new_rows) {
  if (empty($new_rows) || (int) $entity->status !== 1) {
    return;
  }

  $queue = DrupalQueue::get('modularform_mail_queue');

  $accounts = user_load_multiple(array_column($new_rows, 'uid'));
  foreach ($new_rows as $row) {
    $account = isset($accounts[$row['uid']]) ? $accounts[$row['uid']] : NULL;
    if (!$account || empty($account->mail)) {
      continue;
    }

    $form_url = url('forms/' . $entity->id . '/fill', [
      'absolute' => TRUE,
      'query'    => ['token' => $row['token']],
    ]);

    $queue->createItem([
      'key'    => 'form_notify_user',
      'to'     => $account->mail,
      'params' => [
        'form'     => $entity->title,
        'account'  => $account,
        'form_url' => $form_url,
      ],
    ]);
  }
}

/**
 * Implements hook_entity_update().
 */
function modularform_entity_update($entity, $type) {
  if ($type !== 'modularform_form') {
    return;
  }
  if ((int) $entity->status !== 1 || (int) $entity->original->status === 1) {
    return;
  }

  $now = (new DateTime('now', new DateTimeZone('UTC')))->getTimestamp();
  $rows = db_select('modularform_form_members', 'm')
    ->fields('m', ['uid', 'token'])
    ->condition('m.form_id', $entity->id)
    ->condition('m.roles', 2)        // participant
    ->execute()
    ->fetchAll(PDO::FETCH_ASSOC);

  _modularform_queue_restricted_notifications($entity->id, $entity, $rows);

  _modularform_queue_private_notifications($entity->id, $entity, $now);
}
