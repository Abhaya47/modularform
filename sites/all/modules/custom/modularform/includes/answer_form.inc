<?php

require_once dirname(__FILE__) . '/builder_build_fields.inc';
module_load_include('inc', 'modularform', 'modularform.rules');

function modularform_answer_form($form, &$form_state, $fid) {
  $elements = _modularform_load_elements($fid);

  _modularform_build_fields($form, $elements, 'submit');

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Submit'),
  ];
  return $form;
}

function modularform_answer_form_submit($form, &$form_state) {
  global $user;
  $values = $form_state['values'];
  $fid = $form_state['build_info']['args'][0];
  $elements = _modularform_load_elements($fid);

  $now = REQUEST_TIME;

  $multi_types = ['checkbox', 'tag', 'list'];
  $skip = ['submit', 'form_build_id', 'form_token', 'form_id', 'op'];

  // Load the form record to check anonymous_responses
  $form_record = db_select('modularform_forms', 'f')
    ->fields('f', ['anonymous_responses'])
    ->condition('f.id', $fid)
    ->execute()
    ->fetchAssoc();

  $is_anonymous_form = !empty($form_record['anonymous_responses']);

  if ($is_anonymous_form) {
    $email = NULL;
  }
  elseif ($user->uid) {
    $email = $user->mail;
  }
  else {
    $email = isset($values['_guest_email']) ? trim($values['_guest_email']) : '';
  }

  $fields = [
    'form_id' => $fid,
    'user_id' => $user->uid,
    'submitted_at' => $now,
    'created_at' => $now,
    'updated_at' => $now,
  ];

  if (!$is_anonymous_form) {
    $fields['email'] = $email;
  }

  // Catch duplicate submission at the DB constraint level.
  try {
    $response_id = db_insert('modularform_responses')
      ->fields($fields)
      ->execute();
  }
  catch (PDOException $e) {
    // 23000 is the SQLSTATE code for integrity constraint violation (duplicate key).
    if ($e->getCode() == 23000) {
      form_set_error('', t('You have already submitted this form.'));
      $form_state['rebuild'] = TRUE;
      return;
    }
    // Rethrow anything unexpected.
    throw $e;
  }

  // --- 2. Iterate sections and questions ---
  foreach ($values as $sec_key => $section) {
    if (in_array($sec_key, $skip) || !is_array($section)) {
      continue;
    }

    foreach ($section as $q_id => $q_data) {
      if (!isset($elements[$q_id])) {
        continue;
      }

      $raw = $q_data['answer'];
      $q_type = $elements[$q_id]['question_type'];
      $is_multi = in_array($q_type, $multi_types);

      // --- 3. Build answer row ---
      $answer_row = [
        'response_id' => $response_id,
        'q_id' => $q_id,
        'datatype' => modularform_type_map()[$q_type],
        'value_varchar' => NULL,
        'value_text' => NULL,
        'value_int' => NULL,
        'value_bool' => NULL,
        'value_date' => NULL,
        'created_at' => $now,
        'updated_at' => $now,
      ];

      if (!$is_multi) {
        switch ($q_type) {
          case 'text':
            $answer_row['value_varchar'] = (string) $raw;
            break;

          case 'textarea':
          case 'richtext':
            $answer_row['value_text'] = (string) $raw;
            break;

          case 'radio':
          case 'dropdown':
            $answer_row['value_varchar'] = $raw;
            break;

          case 'boolean':
            $answer_row['value_bool'] = $raw ? 1 : 0;
            break;

          case 'date':
          case 'time':

            if (is_array($raw)) {
              if (!empty($raw['date'])) {
                $raw = $raw['date'];
              }
              elseif (!empty($raw['value'])) {
                $raw = $raw['value'];
              }
              else {
                $raw = implode(' ', array_filter($raw));
              }
            }

            $raw = trim((string) $raw);

            // Parse the date first
            $parsed_date = strtotime($raw);

            // Check that it's not empty AND that strtotime didn't fail (return false)
            $answer_row['value_date'] = ($raw !== '' && $parsed_date !== FALSE)
              ? $parsed_date
              : NULL;

            break;
        }
      }

      $answer_id = db_insert('modularform_answers')
        ->fields($answer_row)
        ->execute();

      // --- 4. Multi-value: parse and insert child rows ---
      if ($is_multi && !empty($raw)) {
        // Form API may deliver $raw as an array (checkboxes, multi-select)
        // or as a comma-separated string (hidden/text fields). Normalise first.
        $parts = is_array($raw)
          ? array_values(array_filter($raw, fn($v) => $v !== '' && $v !== NULL))
          : array_filter(array_map('trim', explode(',', $raw)), fn($v) => $v !== '');

        foreach ($parts as $part) {
          $part = trim($part);

          if ($q_type === 'tag') {
            if (strpos($part, 'new:') === 0) {
              $tag_labels = explode(',', substr($part, 4));
              $resolved_ids = _modularform_insert_new_tags($q_id, $tag_labels, $now);

              foreach ($resolved_ids as $tag_id) {
                db_insert('modularform_answers_multi')
                  ->fields(['answer_id' => $answer_id, 'value' => $tag_id])
                  ->execute();
              }
              continue; // ← skip the single insert below
            }
            else {
              $resolved = (int) $part;
            }
          }
          else {
            $resolved = $part;
          }

          // Only reached for non-"new:" cases
          db_insert('modularform_answers_multi')
            ->fields(['answer_id' => $answer_id, 'value' => $resolved])
            ->execute();
        }
      }
    }
  }
}

function modularform_response_edit_form($form, &$form_state, $response_id) {
  $response = db_select('modularform_responses', 'r')
    ->fields('r')
    ->condition('r.id', $response_id)
    ->execute()
    ->fetchObject();

  if (!$response) {
    drupal_set_message(t('Response not found.'), 'error');
    drupal_goto('forms/response/list');
  }

  // Load the parent form
  $form_record = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'editable'])
    ->condition('f.id', $response->form_id)
    ->execute()
    ->fetchObject();

  if (!$form_record) {
    drupal_set_message(t('Parent form not found.'), 'error');
    drupal_goto('forms/response/list');
  }

  // Only allow editing if the form supports it
  if (!$form_record->editable) {
    drupal_set_message(t('This form does not allow editing responses.'), 'warning');
    drupal_goto('forms/response/view/' . $response_id);
  }

  $form_state['storage']['response_id'] = $response_id;
  $form_state['storage']['form_id'] = $response->form_id;

  $elements = _modularform_load_elements($response->form_id);
  $defaults = _modularform_load_defaults($response_id);
  _modularform_build_fields($form, $elements, 'edit', $defaults);

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Save changes'),
  ];
  return $form;
}

function modularform_response_edit_form_submit($form, &$form_state) {
  $response_id = $form_state['storage']['response_id'];
  $form_id = $form_state['storage']['form_id'];
  $elements = _modularform_load_elements($form_id);
  $values = $form_state['values'];
  $now = REQUEST_TIME;

  $sections = array_filter($elements, fn($el) => $el['type'] === 'section');
  $questions = array_filter($elements, fn($el) => $el['type'] === 'question');

  $transaction = db_transaction();

  try {
    foreach ($sections as $sec_id => $sec) {
      $sec_questions = array_filter(
        $questions,
        fn($q) => $q['parent'] === $sec_id
      );

      foreach ($sec_questions as $q_id => $q) {
        if (!isset($values[$sec_id][$q_id]['answer'])) {
          continue;
        }

        $raw = $values[$sec_id][$q_id]['answer'];
        $q_type = $q['question_type'];

        // Base row values.
        $update = [
          'updated_at' => $now,
          'value_varchar' => NULL,
          'value_text' => NULL,
          'value_int' => NULL,
          'value_bool' => NULL,
          'value_date' => NULL,
        ];

        switch ($q_type) {
          case 'text':
          case 'radio':
          case 'dropdown':
            $update['value_varchar'] = (string) $raw;
            break;

          case 'textarea':
          case 'richtext':
            $update['value_text'] = (string) $raw;
            break;

          case 'boolean':
            $update['value_bool'] = ($raw === 'yes') ? 1 : 0;
            break;

          case 'date':
          case 'time':

            if (is_array($raw)) {
              $raw = $raw['date']
                ?? $raw['value']
                ?? implode(' ', array_filter($raw));
            }

            $raw = trim((string) $raw);
            $parsed = strtotime($raw);

            $update['value_date'] = (
              $raw !== '' && $parsed !== FALSE
            ) ? $parsed : NULL;

            break;
        }

        // Save main answer row first.
        db_merge('modularform_answers')
          ->key([
            'response_id' => $response_id,
            'q_id' => $q_id,
          ])
          ->insertFields([
              'response_id' => $response_id,
              'q_id' => $q_id,
              'datatype' => modularform_type_map()[$q_type],
              'created_at' => $now,
            ] + $update)
          ->updateFields($update)
          ->execute();

        // Get answer ID after merge.
        $answer_id = db_select('modularform_answers', 'a')
          ->fields('a', ['id'])
          ->condition('a.response_id', $response_id)
          ->condition('a.q_id', $q_id)
          ->execute()
          ->fetchField();

        // Handle multi-value fields.
        if (in_array($q_type, ['checkbox', 'list'])) {
          $parts = is_array($raw)
            ? array_keys(array_filter($raw))
            : array_filter(explode(',', (string) $raw));

          db_delete('modularform_answers_multi')
            ->condition('answer_id', $answer_id)
            ->execute();

          foreach ($parts as $val) {
            db_insert('modularform_answers_multi')
              ->fields([
                'answer_id' => $answer_id,
                'value' => trim($val),
              ])
              ->execute();
          }
        }
        elseif ($q_type === 'tag') {
          $parts = is_array($raw)
            ? array_values(array_filter($raw, fn($v) => $v !== '' && $v !== NULL))
            : array_filter(array_map('trim', explode(',', (string) $raw)), fn($v) => $v !== '');

          $new_tags = [];
          $existing_ids = [];

          foreach ($parts as $part) {
            $part = trim($part);
            if (strpos($part, 'new:') === 0) {
              $new_tags[] = substr($part, 4); // collect, don't insert yet
            }
            else {
              $existing_ids[] = (int) $part;
            }
          }

          $new_ids = !empty($new_tags)
            ? _modularform_insert_new_tags($q_id, $new_tags, $now)
            : [];

          $all_ids = array_merge($existing_ids, $new_ids);

          db_delete('modularform_answers_multi')
            ->condition('answer_id', $answer_id)
            ->execute();

          foreach ($all_ids as $tag_id) {
            db_insert('modularform_answers_multi')
              ->fields(['answer_id' => $answer_id, 'value' => $tag_id])
              ->execute();
          }
        }
      }
    }

    db_update('modularform_responses')
      ->fields([
        'updated_at' => $now,
      ])
      ->condition('id', $response_id)
      ->execute();

    drupal_set_message(t('Response updated successfully.'));
    $form_state['redirect'] = 'forms/response/list';
  }

  catch (Exception $e) {
    $transaction->rollback();

    watchdog_exception('modularform', $e);

    drupal_set_message(
      t('An error occurred while saving. Please try again.'),
      'error'
    );
  }
}

function _modularform_load_defaults($response_id) {
  $answers = db_select('modularform_answers', 'a')
    ->fields('a', [
      'id',
      'q_id',
      'datatype',
      'value_varchar',
      'value_text',
      'value_bool',
      'value_date',
    ])
    ->condition('a.response_id', $response_id)
    ->execute()
    ->fetchAllAssoc('q_id');

  $type_map = array_flip(modularform_type_map()); // [0 => 'text', 1 => 'textarea', …]
  $defaults = [];

  foreach ($answers as $q_id => $answer) {
    $type = $type_map[(int) $answer->datatype] ?? 'text';

    switch ($type) {
      case 'text':
      case 'radio':
      case 'dropdown':
        $defaults[$q_id] = $answer->value_varchar;

        break;

      case 'textarea':
      case 'richtext':
        $defaults[$q_id] = $answer->value_text;
        break;

      case 'time':
        $defaults[$q_id] = $answer->value_varchar;
        break;

      case 'boolean':
        $defaults[$q_id] = $answer->value_bool ? 'yes' : 'no';
        break;

      case 'date':
        $defaults[$q_id] = $answer->value_date
          ? date('Y-m-d', $answer->value_date)
          : NULL;
        break;

      case 'checkbox':
      case 'list':
        $defaults[$q_id] = db_select('modularform_answers_multi', 'm')
          ->fields('m', ['value'])
          ->condition('m.answer_id', $answer->id)
          ->execute()
          ->fetchCol(); // varchar o_ids
        break;

      case 'tag':
        $defaults[$q_id] = array_map('intval',
          db_select('modularform_answers_multi', 'm')
            ->fields('m', ['value'])
            ->condition('m.answer_id', $answer->id)
            ->execute()
            ->fetchCol()
        ); // int tag ids
        break;
    }
  }

  return $defaults;
}

// Helper now correctly accepts an array
function _modularform_insert_new_tags($q_id, array $labels, $now) {
  $ids = [];
  foreach ($labels as $label) {
    $label = trim($label);
    if ($label === '') {
      continue;
    }
    $id = db_insert('modularform_tags')
      ->fields([
        'title' => $label,
        'created_at' => $now,
      ])
      ->execute(); // returns last insert ID (int) in Drupal 7

    if ($id) {
      $ids[] = (int) $id;
    }
  }
  return $ids;
}
