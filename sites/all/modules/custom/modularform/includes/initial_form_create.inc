<?php

module_load_include('inc', 'modularform', 'modularform.rules');

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

  // ── Editable ──────────────────────────────────────────────────────────────

  $form['editable'] = [
    '#type' => 'checkbox',
    '#title' => t('Allow respondents to edit their submission?'),
    '#default_value' => $val('editable', 0),
    '#weight' => 13,
  ];

  // ── Anonymous responses ───────────────────────────────────────────────────

  $form['anonymous_responses'] = [
    '#type' => 'checkbox',
    '#title' => t('Collect responses anonymously?'),
    '#default_value' => $val('anonymous_responses', 0),
    '#weight' => 14,
  ];

  // ── Visibility ────────────────────────────────────────────────────────────
  if ($id) {
    $form['visibility'] = [
      '#type' => 'select',
      '#title' => t('Visibility'),
      '#options' => [
        0 => t('Public'),
        1 => t('Restricted (by role)'),
        2 => t('Private (specific people)'),
      ],
      '#required' => FALSE,
      '#default_value' => $val('visibility', 0),
      '#ajax' => [
        'callback' => 'modularform_visibility_ajax',
        'wrapper' => 'visibility-options-wrapper',
      ],
    ];

    $form['visibility_options_wrapper'] = [
      '#type' => 'container',
      '#attributes' => ['id' => 'visibility-options-wrapper'],
    ];

    $visibility = $val('visibility', 0);
    $text_map = modularform_visibility_get_text_map();
    $visibility_text = isset($text_map[$visibility]) ? $text_map[$visibility] : 'public';

    if ($visibility_text === 'restricted') {
      $roles = array_diff_key(user_roles(TRUE), [DRUPAL_AUTHENTICATED_RID => TRUE]);
      $form['visibility_options_wrapper']['roles'] = [
        '#type' => 'checkboxes',
        '#title' => t('Allowed roles'),
        '#options' => $roles,
        '#required' => TRUE,
        '#default_value' => $val('roles', []),
        '#description' => t('Members of your organization with these roles can access this form.'),
      ];
    }

    if ($visibility_text === 'private') {
      // ── Load existing members + pending invites ──────────────────────────
      $existing_members = db_select('modularform_form_members', 'm')
        ->fields('m', ['uid', 'token', 'roles'])  // also fetch roles
        ->condition('m.form_id', $id)
        ->condition('m.uid', 0, '>')
        ->execute()
        ->fetchAll(PDO::FETCH_ASSOC);

      $existing_invites = db_select('modularform_form_invites', 'i')
        ->fields('i', ['email', 'token'])
        ->condition('i.form_id', $id)
        ->condition('i.status', 0)   // 0 = pending, not yet consumed
        ->execute()
        ->fetchAll(PDO::FETCH_ASSOC);

      // Build display list: [['label' => '...', 'value' => 'uid:X or email:Y'], ...]
      $people_list = [];

      if (!empty($existing_members)) {
        $uids = array_column($existing_members, 'uid');
        $accounts = user_load_multiple($uids);
        foreach ($existing_members as $row) {
          $account = isset($accounts[$row['uid']]) ? $accounts[$row['uid']] : NULL;
          if (!$account) {
            continue;
          }
          $people_list[] = [
            'label' => $account->name . ' (' . $account->mail . ')',
            'value' => 'uid:' . $row['uid'],
            'role' => (int) $row['roles'],  // load existing role
          ];
        }
      }

      foreach ($existing_invites as $row) {
        $people_list[] = [
          'label' => $row['email'],
          'value' => 'email:' . $row['email'],
        ];
      }

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

      // Existing people rendered as a deletable list
      if (!empty($people_list)) {
        $form['visibility_options_wrapper']['people_wrapper']['existing_list'] = [
          '#markup' => theme('modularform_people_list', ['people' => $people_list]),
        ];
      }

      // Visible anchor input — Select2 attaches here
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

      // Hidden field carries surviving + new entries; JS manages it
      // Pre-populated with current members so absence-equals-delete works on submit
      $initial_value = implode(',', array_map(function($p) {
        $role = isset($p['role']) ? (int) $p['role'] : 2;
        return $p['value'] . ':' . $role;
      }, $people_list));
      $form['visibility_options_wrapper']['people_wrapper']['people_value'] = [
        '#type' => 'hidden',
        '#default_value' => $initial_value,
        '#attributes' => [
          'id' => 'modularform-people-input',
          'class' => ['modularform-people-hidden'],
        ],
      ];

      $form['visibility_options_wrapper']['people_wrapper']['#attached']['library'][] = [
        'modularform',
        'select2',
      ];
      $form['visibility_options_wrapper']['people_wrapper']['#attached']['js'][drupal_get_path('module', 'modularform') . '/js/modularform.people.js'] = [
        'type'   => 'file',
        'weight' => 10,
      ];


    }
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

  if ($id) {
    $form['submit'] = [
      '#type' => 'submit',
      '#value' => t('Save settings'),
      '#weight' => 200,
    ];
  }
  else {
    $form['submit'] = [
      '#type' => 'submit',
      '#value' => t('Start Building'),
      '#weight' => 200,
    ];
  }

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
  $entity->editable = !empty($values['editable']) ? 1 : 0;
  $entity->anonymous_responses = !empty($values['anonymous_responses']) ? 1 : 0;
  // ── visibility ────────────────────────────────────────────────────────────
  $visibility = $values['visibility'];
  $entity->visibility = $visibility;
  $form_id = $entity->id;

  $text_map = modularform_visibility_get_text_map();
  $visibility_text = isset($text_map[$values['visibility']]) ? $text_map[$values['visibility']] : 'public';

  if ($visibility_text === 'restricted') {
    $selected_rids = array_filter($values['visibility_options_wrapper']['roles']);

    $uids_to_add = [];
    if (isset($is_editing)) {
      $uids_to_add = _modularform_restricted_people_edit($form_id, $selected_rids, $entity, $now);
    }

    _modularform_restricted_people_add($form_id, $selected_rids, $entity, $now, $uids_to_add);

    // If already published, notify any unnotified members immediately.
    if (isset($is_editing) && (int) $entity->status === 1) {
      $rows = db_select('modularform_form_members', 'm')
        ->fields('m', ['uid', 'token'])
        ->condition('m.form_id', $form_id)
        ->condition('m.roles', 2)
        ->condition('m.notified', 0)
        ->execute()
        ->fetchAll(PDO::FETCH_ASSOC);

      if (!empty($rows)) {
        _modularform_queue_restricted_notifications($form_id, $entity, $rows);
        db_update('modularform_form_members')
          ->fields(['notified' => 1, 'updated_at' => $now])
          ->condition('form_id', $form_id)
          ->condition('uid', array_column($rows, 'uid'), 'IN')
          ->execute();
      }
    }
  }

  elseif ($visibility_text === 'private') {
    $raw = $values['visibility_options_wrapper']['people_wrapper']['people_value'] ?? '';

    if (isset($is_editing)) {
      _modularform_edit_private_people($form_id, $raw, $entity, $now);
    }

    $inserted = _modularform_save_private_people($form_id, $raw, $entity, $now);

    if (isset($is_editing) && (int) $entity->status === 1) {
      // Notify privileged roles immediately
      _modularform_notify_privileged_members($form_id, $entity, $inserted, $now);
      // Notify remaining unnotified members/invites via normal path
      _modularform_queue_private_notifications($form_id, $entity, $now);
    }
  }

  $entity->save();

  $message = !empty($id) ? 'Form %title updated.' : 'Form %title saved in draft.';
  drupal_set_message(t($message, ['%title' => $entity->title]));
  if ($id) {
    drupal_goto('forms/manage');
  }
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
  $search = isset($_GET['people']) ? trim(check_plain($_GET['people'])) : '';

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
 * @param int $form_id
 * @param array $selected_rids
 * @param object $entity
 * @param int $now
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
  $to_keep = array_intersect($existing_uids, $new_uids);
  $to_add = array_diff($new_uids, $existing_uids);

  // Remove members no longer in the role set.
  if (!empty($to_remove)) {
    #TODO make an apology
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
 * On an edit flow, pass only the diff returned by
 * _modularform_restricted_people_edit().
 *
 * @param int $form_id
 * @param array $selected_rids (unused here, kept for signature consistency)
 * @param object $entity
 * @param int $now
 * @param array $uids_to_add Explicit uid list; if empty the function
 *                               resolves it from $selected_rids itself (create
 *   path).
 */
function _modularform_restricted_people_add($form_id, array $selected_rids, $entity, $now, array $uids_to_add = []) {
  // On create path: resolve from roles. On edit path: diff is passed in directly.
  if (empty($uids_to_add)) {
    $uids_to_add = _modularform_get_protected_people($selected_rids);
  }

  if (empty($uids_to_add)) {
    return;
  }

  $query = db_insert('modularform_form_members')
    ->fields([
      'form_id',
      'uid',
      'roles',
      'status',
      'token',
      'notified',
      'created_at',
      'updated_at',
    ]);

  $new_rows = [];
  foreach ($uids_to_add as $uid) {
    $token = hash('sha256', $form_id . '_' . $uid . '_' . drupal_random_key(16));
    $new_rows[] = ['uid' => (int) $uid, 'token' => $token];

    $query->values([
      'form_id' => $form_id,
      'uid' => (int) $uid,
      'roles' => 2,
      'status' => 0,
      'token' => $token,
      'notified' => 0,   // notifications only fire on publish
      'created_at' => $now,
      'updated_at' => $now,
    ]);
  }

  $query->execute();
  // No notification queuing here — entity_update handles that on publish.
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

    // Format is uid:X:ROLE or email:addr:ROLE
    // Split on last colon to get role
    $last_colon = strrpos($item, ':');
    $role = 2;  // default
    if ($last_colon !== FALSE) {
      $maybe_role = substr($item, $last_colon + 1);
      if (is_numeric($maybe_role) && in_array((int) $maybe_role, [0, 1, 2])) {
        $role = (int) $maybe_role;
        $item = substr($item, 0, $last_colon);
      }
    }

    if (strpos($item, 'uid:') === 0) {
      $uid = (int) substr($item, 4);
      if ($uid > 0) {
        $result['drupal'][] = ['uid' => $uid, 'role' => $role];
      }
    }
    elseif (strpos($item, 'email:') === 0) {
      $email = trim(substr($item, 6));
      if (valid_email_address($email)) {
        $result['external'][] = $email;  // plain string, role is always participant
      }
    }
  }

  return $result;
}

/**
 * Inserts member rows for private Drupal users not already in the table.
 *
 * @param int $form_id
 * @param array $uids
 * @param int $now
 *
 * @return array
 *   Uids that were actually inserted (not already existing).
 */
function _modularform_save_private_drupal($form_id, array $people, $now) {
  // $people is now [['uid' => int, 'role' => int], ...]
  if (empty($people)) {
    return [];
  }

  $uids = array_column($people, 'uid');

  $existing_uids = db_select('modularform_form_members', 'm')
    ->fields('m', ['uid'])
    ->condition('m.form_id', $form_id)
    ->condition('m.uid', $uids, 'IN')
    ->execute()
    ->fetchCol();

  $to_insert = array_values(array_filter($people, function($p) use ($existing_uids) {
    return !in_array($p['uid'], $existing_uids);
  }));

  if (empty($to_insert)) {
    return [];
  }

  $query = db_insert('modularform_form_members')
    ->fields([
      'form_id',
      'uid',
      'roles',
      'status',
      'token',
      'notified',
      'created_at',
      'updated_at',
    ]);

  $inserted = [];
  foreach ($to_insert as $p) {
    $token = hash('sha256', $form_id . '_' . $p['uid'] . '_' . drupal_random_key(16));
    $query->values([
      'form_id' => $form_id,
      'uid' => (int) $p['uid'],
      'roles' => (int) $p['role'],
      'status' => 0,
      'token' => $token,
      'notified' => 0,
      'created_at' => $now,
      'updated_at' => $now,
    ]);
    $inserted[] = [
      'uid' => (int) $p['uid'],
      'token' => $token,
      'role' => (int) $p['role'],
    ];
  }

  $query->execute();
  return $inserted;
}

/**
 * Queues immediate notifications for newly inserted creator/viewer members.
 * Called only when a form is already published and new non-participant
 * members are added.
 *
 * @param int $form_id
 * @param object $entity
 * @param array $inserted
 *   Array of ['uid' => int, 'token' => string, 'role' => int].
 * @param int $now
 */
function _modularform_notify_privileged_members($form_id, $entity, array $inserted, $now) {
  if (empty($inserted) || (int) $entity->status !== 1) {
    return;
  }

  // Only notify creators (0) and viewers (1) immediately.
  // Participants go through the normal notified=0 queue path.
  $privileged = array_filter($inserted, function($r) {
    return in_array((int) $r['role'], [0, 1]);
  });

  if (empty($privileged)) {
    return;
  }

  $queue    = DrupalQueue::get('modularform_mail_queue');
  $accounts = user_load_multiple(array_column($privileged, 'uid'));

  foreach ($privileged as $row) {
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
        'form'     => $entity->title,
        'account'  => $account,
        'form_url' => $form_url,
        'role'     => $row['role'],  // mail handler can customise subject by role
      ],
    ]);
  }

  // Mark them notified immediately
  db_update('modularform_form_members')
    ->fields(['notified' => 1, 'updated_at' => $now])
    ->condition('form_id', $form_id)
    ->condition('uid', array_column($privileged, 'uid'), 'IN')
    ->execute();
}

/**
 * Inserts invite rows for external emails not already invited.
 *
 * @param int $form_id
 * @param array $emails
 * @param int $now
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

  $to_insert = array_values(array_diff($emails, $existing_emails));

  if (empty($to_insert)) {
    return [];
  }

  $query = db_insert('modularform_form_invites')
    ->fields(['form_id', 'email', 'token', 'status', 'notified', 'created_at']);

  foreach ($to_insert as $email) {
    $token = hash('sha256', $form_id . '_' . $email . '_' . drupal_random_key(16));
    $query->values([
      'form_id' => $form_id,
      'email' => $email,
      'token' => $token,
      'status' => 0,   // 0 = pending, not yet accepted
      'notified' => 0,
      'created_at' => $now,
    ]);
  }

  $query->execute();

  return array_values($to_insert);
}

/**
 * Queues notification emails for all unnotified private members and invites.
 *
 * @param int $form_id
 * @param object $entity
 * @param int $now
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
        'query' => ['token' => $row['token']],
      ]);

      $queue->createItem([
        'key' => 'form_notify_user',
        'to' => $account->mail,
        'params' => [
          'form_id' => $entity->title,
          'uid' => $account->uid,
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
        'key' => 'form_invite_external',
        'to' => $row['email'],
        'params' => [
          'form_id' => $entity->id,
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
 * Saves private people on create or edit, and queues notifications if
 * published.
 *
 * @param int $form_id
 * @param string $raw
 * @param object $entity
 * @param int $now
 */
function _modularform_save_private_people($form_id, $raw, $entity, $now) {
  $parsed   = _modularform_parse_private_people($raw);
  $inserted = _modularform_save_private_drupal($form_id, $parsed['drupal'], $now);
  _modularform_save_private_external($form_id, $parsed['external'], $now);
  return $inserted;  // only Drupal members; invites have no immediate role notification
}

/**
 * Syncs private people on edit — removes dropped members, adds new ones.
 *
 * @param int $form_id
 * @param string $raw
 * @param object $entity
 * @param int $now
 */
function _modularform_edit_private_people($form_id, $raw, $entity, $now) {
  $parsed = _modularform_parse_private_people($raw);

  // ── Drupal users ───────────────────────────────────────────────────────
  $new_uids = array_column($parsed['drupal'], 'uid');  // extract flat uid list

  $existing_uids = db_select('modularform_form_members', 'm')
    ->fields('m', ['uid'])
    ->condition('m.form_id', $form_id)
    ->condition('m.roles', 2)
    ->execute()
    ->fetchCol();

  $to_remove = array_diff($existing_uids, $new_uids);  // now comparing flat vs flat

  if (!empty($to_remove)) {
    db_delete('modularform_form_members')
      ->condition('form_id', $form_id)
      ->condition('uid', $to_remove, 'IN')
      ->condition('roles', 2)
      ->execute();
  }

  _modularform_save_private_drupal($form_id, $parsed['drupal'], $now);

  // ── External invites ───────────────────────────────────────────────────
  $existing_emails = db_select('modularform_form_invites', 'i')
    ->fields('i', ['email'])
    ->condition('i.form_id', $form_id)
    ->condition('i.status', 0)
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
}

/**
 * Queues notification emails for newly added restricted members.
 *
 * @param int $form_id
 * @param object $entity
 * @param array $new_rows
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
      'query' => ['token' => $row['token']],
    ]);

    $queue->createItem([
      'key' => 'form_notify_user',
      'to' => $account->mail,
      'params' => [
        'form' => $entity->title,
        'account' => $account,
        'form_url' => $form_url,
      ],
    ]);
  }
}


