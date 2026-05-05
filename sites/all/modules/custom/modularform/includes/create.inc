<?php

function modularform_create_form($form, &$form_state, $id = NULL) {
  // Load existing form entity if editing.
  $modular_form = NULL;
  if ($id) {
    $results = entity_get_controller('modularform_form')->load([$id]);
    $modular_form = reset($results) ?: NULL;
  }

  // Helper to pull saved or submitted value, falling back to entity or default.
  // Using form_state values is what drives the AJAX rebuild correctly.
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
  // 'public'     — anyone can access
  // 'restricted' — specific roles within the organization
  // 'private'    — specific named users (in-system or invited by email)

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
    // Pull roles available within this organization.
    // Filter out Drupal's built-ins — only show org-relevant roles.
    $roles = array_diff_key(user_roles(TRUE), [
      DRUPAL_AUTHENTICATED_RID => TRUE,
    ]);
    $form['visibility_options_wrapper']['roles'] = [
      '#type' => 'checkboxes',        // multi-select via checkboxes
      '#title' => t('Allowed roles'),
      '#options' => $roles,
      '#required' => TRUE,
      '#default_value' => $val('roles', []),
      '#description' => t('Members of your organization with these roles can access this form.'),
    ];
  }

  if ($visibility === 'private') {
    $form['visibility_options_wrapper']['people'] = [
      '#type' => 'fieldset',
      '#title' => t('Add people'),
      '#prefix' => '<div id="people-fieldset-wrapper">',
      '#suffix' => '</div>',
      '#tree' => TRUE,
    ];

    if (empty($form_state['num_people'])) {
      $form_state['num_people'] = 1;
    }

    for ($i = 0; $i < $form_state['num_people']; $i++) {
      $saved = isset($my_saved_people[$i]) ? $my_saved_people[$i] : '';

      $form['visibility_options_wrapper']['people']['user'][$i] = [
        '#type' => 'textfield',
        '#autocomplete_path' => 'user/autocomplete',
        '#title' => $i === 0 ? t('Username') : '',
        '#default_value' => $saved,
      ];
    }

    $form['visibility_options_wrapper']['people']['add_more'] = [
      '#type' => 'submit',
      '#value' => t('Add another person'),
      '#submit' => ['your_form_add_more_submit'],
      '#ajax' => [
        'callback' => 'your_form_add_more_callback',
        'wrapper' => 'people-fieldset-wrapper',
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
    '#weight' => 10,
  ];

  $form['closes_at_wrapper'] = [
    '#type' => 'container',
    '#attributes' => ['id' => 'closes-at-wrapper'],
    '#weight' => 11,
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

function your_form_add_more_submit($form, &$form_state) {
  $form_state['num_people']++;
  $form_state['rebuild'] = TRUE;
}

function your_form_add_more_callback($form, &$form_state) {
  return $form['visibility_options_wrapper']['people'];
}

// ── AJAX callbacks ────────────────────────────────────────────────────────────

function modularform_closes_ajax($form, &$form_state) {
  return $form['closes_at_wrapper'];
}

function modularform_visibility_ajax($form, &$form_state) {
  return $form['visibility_options_wrapper'];
}

//modularform_create_form submit
function modularform_create_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $id = $values['id'];
  $controller = entity_get_controller('modularform_form');
  $now = (new DateTime('now', new DateTimeZone('UTC')))->getTimestamp();

  if (!empty($id)) {
    $results = $controller->load([$id]);
    $entity = reset($results);
  }
  else {
    $entity = $controller->create(['type' => 'organization']);
    $entity->slug = modularform_generate_random_slug($values['title']);
    $entity->created_at = $now;
    global $user;
    $entity->created_by = $user->uid;
  }

  $entity->title = $values['title'];
  $entity->description = $values['description'];
  $entity->status = $values['status'];
  $entity->updated_at = $now;

  // ── closes_at fix ─────────────────────────────────────────────────────────
  $closes_at = 0;
  if (!empty($values['closes']) && !empty($values['closes_at_wrapper']['closes_at'])) {
    $d = $values['closes_at_wrapper']['closes_at'];
    $closes_at = mktime(0, 0, 0, $d['month'], $d['day'], $d['year']);
  }
  $entity->closes = !empty($values['closes']) ? 1 : 0;
  $entity->closes_at = $closes_at;

  $entity->save();

  // ── visibility ────────────────────────────────────────────────────────────
  $visibility = $values['visibility'];
  $form_id = $entity->id;

  // Always clear old members/invites on save so we start fresh
  db_delete('modularform_form_members')
    ->condition('form_id', $form_id)
    ->execute();
  db_delete('modularform_form_invites')
    ->condition('form_id', $form_id)
    ->execute();

  if ($visibility === 'restricted') {
    $selected_rids = array_filter($values['visibility_options_wrapper']['roles']);

    if (!empty($selected_rids)) {
      $queue = DrupalQueue::get('modularform_mail_queue');

      $uids = db_select('users_roles', 'ur')
        ->fields('ur', ['uid'])
        ->condition('ur.rid', $selected_rids, 'IN')
        ->execute()
        ->fetchCol();

      foreach (array_unique($uids) as $uid) {
        db_insert('modularform_form_members')
          ->fields([
            'form_id' => $form_id,
            'uid' => $uid,
            'roles' => 1,
            'created_at' => $now,
            'updated_at' => $now,
          ])
          ->execute();

        $account = user_load($uid);
        if ($account && !empty($account->mail)) {
          $queue->createItem([
            'key' => 'form_notify_role',
            'to' => $account->mail,
            'params' => ['form' => $entity],
          ]);
        }
      }
    }
  }
  elseif ($visibility === 'private') {
    $queue = DrupalQueue::get('modularform_mail_queue');
    $people = array_filter($values['visibility_options_wrapper']['people']['user']);

    foreach ($people as $username) {
      $username = trim($username);
      if (empty($username)) {
        continue;
      }

      if (valid_email_address($username)) {
        db_insert('modularform_form_invites')
          ->fields([
            'form_id' => $form_id,
            'email' => $username,
            'created_at' => $now,
          ])
          ->execute();

        $queue->createItem([
          'key' => 'form_invite',
          'to' => $username,
          'params' => ['form' => $entity],
        ]);
      }
      else {
        $account = user_load_by_name($username);
        if ($account) {
          db_insert('modularform_form_members')
            ->fields([
              'form_id' => $form_id,
              'uid' => $account->uid,
              'roles' => 1,
              'created_at' => $now,
              'updated_at' => $now,
            ])
            ->execute();

          $queue->createItem([
            'key' => 'form_invite_user',
            'to' => $account->mail,
            'params' => ['form' => $entity],
          ]);
        }
      }
    }
  }

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

  // Generate a short 4-character random hex string
  $random_suffix = substr(md5(uniqid(mt_rand(), TRUE)), 0, 4);
  $final_slug = $slug . '_' . $random_suffix;

  // Check if slug exists
  if (_modularform_slug_exists($final_slug)) {
    return modularform_generate_random_slug($title);
  }

  return $final_slug;
}

/**
 * Internal helper to check the database for an existing slug.
 */
function _modularform_slug_exists($slug) {
  //  returns 1 if slug exists using the :slug placeholder for security.
  return db_query("SELECT 1 FROM {modularform_forms} WHERE slug = :slug", [':slug' => $slug])->fetchField();
}


function modularform_soft_delete($form_id) {
  // Validate the form exists.
  $form = db_select('modularform_forms', 'mf')
    ->fields('mf')
    ->condition('id', $form_id)
    ->execute()
    ->fetchObject();

  if (!$form) {
    drupal_set_message(t('Form not found.'), 'error');
    drupal_goto('forms/manage');
  }

  // Mark as deleted by setting a deleted flag/timestamp.
  $updated = db_update('modularform_forms')
    ->fields([
      'deleted_at' => REQUEST_TIME,
    ])
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
