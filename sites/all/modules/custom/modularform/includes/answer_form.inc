<?php

require_once dirname(__FILE__) . '/builder_build_fields.inc';
module_load_include('inc', 'modularform', 'modularform.rules');
module_load_include('inc', 'modularform', 'includes/load_response_data');

function modularform_answer_form($form, &$form_state, $fid) {
  $elements = _modularform_load_elements($fid);
  _modularform_build_fields($form, $elements, $fid, 'submit');

  $form['#cache'] = TRUE;

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
  $skip = ['submit', 'form_build_id', 'form_token', 'form_id', 'op'];

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

  try {
    $response = entity_create('modularform_response', $fields);
    entity_save('modularform_response', $response);
    $response_id = $response->id;
  }
  catch (PDOException $e) {
    if ($e->getCode() == 23000) {
      form_set_error('', t('You have already submitted this form.'));
      $form_state['rebuild'] = TRUE;
      return;
    }
    throw $e;
  }

  $datatype_map = modularform_datatype_map();

  // Pre-build multi question lookup from validation rules.
  $multi_question_ids = [];
  foreach ($elements as $el) {
    if ($el['type'] === 'validation_rule'
      && $el['rule_type'] === 'is_multiple'
      && (int) $el['rule_value'] === 1
    ) {
      $multi_question_ids[$el['parent']] = TRUE;
    }
  }

  // Collectors for bulk child inserts.
  $multi_inserts = [];
  $file_inserts = [];

  foreach ($values as $sec_key => $section) {
    if (in_array($sec_key, $skip) || !is_array($section)) {
      continue;
    }

    foreach ($section as $q_id => $q_data) {
      if (!isset($elements[$q_id])) {
        continue;
      }

      $q_type = $elements[$q_id]['question_type'];
      $storage = $datatype_map[$q_type] ?? 'varchar';
      $is_multi = $storage === 'multi';

      // --- File questions ---
      if ($q_type === 'file') {
        $is_multiple = !empty($multi_question_ids[$q_id]);

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

        $answer_id = db_insert('modularform_answers')
          ->fields($answer_row)
          ->execute();

        $uploaded = _modularform_save_answer_files($q_id, $is_multiple, $response_id, $answer_id, $now);
        foreach ($uploaded as $file) {
          $file_inserts[] = [
            'answer_id' => $answer_id,
            'fid' => $file->fid,
            'created_at' => $now,
          ];
        }
        continue;
      }

      $raw = $q_data['answer'];

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
        $answer_row = array_merge($answer_row, _modularform_map_answer_value($q_type, $raw));
      }

      $answer_id = db_insert('modularform_answers')
        ->fields($answer_row)
        ->execute();

      if ($is_multi && !empty($raw)) {
        $parts = is_array($raw)
          ? array_values(array_filter($raw, fn($v) => $v !== '' && $v !== NULL && $v !== 0 && $v !== '0'))
          : array_filter(array_map('trim', explode(',', $raw)), fn($v) => $v !== '');

        foreach ($parts as $part) {
          $part = trim($part);

          if ($q_type === 'tag') {
            if (strpos($part, 'new:') === 0) {
              $tag_labels = explode(',', substr($part, 4));
              $resolved_ids = _modularform_insert_new_tags($q_id, $tag_labels, $now);
              foreach ($resolved_ids as $tag_id) {
                $multi_inserts[] = [
                  'answer_id' => $answer_id,
                  'value' => $tag_id,
                ];
              }
              continue;
            }
            else {
              $resolved = (int) $part;
            }
          }
          else {
            $resolved = $part;
          }

          $multi_inserts[] = ['answer_id' => $answer_id, 'value' => $resolved];
        }
      }
    }
  }

  // Bulk insert multi child rows.
  if (!empty($multi_inserts)) {
    $query = db_insert('modularform_answers_multi')->fields([
      'answer_id',
      'value',
    ]);
    foreach ($multi_inserts as $row) {
      $query->values($row);
    }
    $query->execute();
  }

  // Bulk insert file child rows.
  if (!empty($file_inserts)) {
    $query = db_insert('modularform_answer_files')->fields([
      'answer_id',
      'fid',
      'created_at',
    ]);
    foreach ($file_inserts as $row) {
      $query->values($row);
    }
    $query->execute();
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

  $form_record = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'editable'])
    ->condition('f.id', $response->form_id)
    ->execute()
    ->fetchObject();

  if (!$form_record) {
    drupal_set_message(t('Parent form not found.'), 'error');
    drupal_goto('forms/response/list');
  }

  if (!$form_record->editable) {
    drupal_set_message(t('This form does not allow editing responses.'), 'warning');
    drupal_goto('forms/response/view/' . $response_id);
  }

  $form_state['storage']['response_id'] = $response_id;
  $form_state['storage']['form_id'] = $response->form_id;

  $elements = _modularform_load_elements($response->form_id);
  $defaults = _modularform_load_defaults($response_id);
  _modularform_build_fields($form, $elements, $response->form_id, 'edit', $defaults);

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
  $datatype_map = modularform_datatype_map();
  $type_map = modularform_type_map();

  // Pre-build multi question lookup from validation rules.
  $multi_question_ids = [];
  foreach ($elements as $el) {
    if ($el['type'] === 'validation_rule'
      && $el['rule_type'] === 'is_multiple'
      && (int) $el['rule_value'] === 1
    ) {
      $multi_question_ids[$el['parent']] = TRUE;
    }
  }

  // Fetch all existing answer ids for this response in one query.
  // This avoids doing db_merge() and then db_select() per question.
  $answer_ids = [];
  $question_ids = array_keys($questions);

  if (!empty($question_ids)) {
    $existing_answers = db_select('modularform_answers', 'a')
      ->fields('a', ['id', 'q_id'])
      ->condition('a.response_id', $response_id)
      ->condition('a.q_id', $question_ids, 'IN')
      ->execute();

    foreach ($existing_answers as $row) {
      $answer_ids[(int) $row->q_id] = (int) $row->id;
    }
  }

  $transaction = db_transaction();

  try {
    foreach ($sections as $sec_id => $sec) {
      $sec_questions = array_filter($questions, fn($q) => $q['parent'] === $sec_id);

      foreach ($sec_questions as $q_id => $q) {
        $q_type = $q['question_type'];
        $storage = $datatype_map[$q_type] ?? 'varchar';

        // --- File questions ---
        if ($q_type === 'file') {
          $is_multiple = !empty($multi_question_ids[$q_id]);

          if (!empty($answer_ids[$q_id])) {
            db_update('modularform_answers')
              ->fields([
                'updated_at' => $now,
              ])
              ->condition('id', $answer_ids[$q_id])
              ->execute();

            $answer_id = $answer_ids[$q_id];
          }
          else {
            $answer_id = db_insert('modularform_answers')
              ->fields([
                'response_id' => $response_id,
                'q_id' => $q_id,
                'datatype' => $type_map[$q_type],
                'created_at' => $now,
                'updated_at' => $now,
                'value_varchar' => NULL,
                'value_text' => NULL,
                'value_int' => NULL,
                'value_bool' => NULL,
                'value_date' => NULL,
              ])
              ->execute();

            $answer_ids[$q_id] = $answer_id;
          }

          $upload_key = 'file_' . $q_id;
          $has_upload = !empty($_FILES['files']['name'][$upload_key]);

          if ($has_upload) {
            $existing_fids = db_select('modularform_answer_files', 'af')
              ->fields('af', ['fid'])
              ->condition('af.answer_id', $answer_id)
              ->execute()
              ->fetchCol();

            foreach ($existing_fids as $efid) {
              $old_file = file_load($efid);
              if ($old_file) {
                file_usage_delete($old_file, 'modularform', 'response', $response_id);
                file_delete($old_file);
              }
            }

            db_delete('modularform_answer_files')
              ->condition('answer_id', $answer_id)
              ->execute();

            $uploaded = _modularform_save_answer_files($q_id, $is_multiple, $response_id, $answer_id, $now);

            foreach ($uploaded as $file) {
              db_insert('modularform_answer_files')
                ->fields([
                  'answer_id' => $answer_id,
                  'fid' => $file->fid,
                  'created_at' => $now,
                ])
                ->execute();
            }
          }

          continue;
        }

        // --- Non-file questions ---
        if (!isset($values[$sec_id][$q_id]['answer'])) {
          continue;
        }

        $raw = $values[$sec_id][$q_id]['answer'];

        $update = array_merge(
          ['updated_at' => $now],
          _modularform_map_answer_value($q_type, $raw)
        );

        if (!empty($answer_ids[$q_id])) {
          db_update('modularform_answers')
            ->fields($update)
            ->condition('id', $answer_ids[$q_id])
            ->execute();

          $answer_id = $answer_ids[$q_id];
        }
        else {
          $answer_id = db_insert('modularform_answers')
            ->fields([
                'response_id' => $response_id,
                'q_id' => $q_id,
                'datatype' => $type_map[$q_type],
                'created_at' => $now,
              ] + $update)
            ->execute();

          $answer_ids[$q_id] = $answer_id;
        }

        if ($storage === 'multi') {
          if ($q_type === 'tag') {
            $parts = is_array($raw)
              ? array_values(array_filter($raw, fn($v) => $v !== '' && $v !== NULL))
              : array_filter(array_map('trim', explode(',', (string) $raw)), fn($v) => $v !== '');

            $new_tags = [];
            $existing_ids = [];

            foreach ($parts as $part) {
              $part = trim($part);

              if (strpos($part, 'new:') === 0) {
                $new_tags[] = substr($part, 4);
              }
              else {
                $existing_ids[] = (string) (int) $part;
              }
            }

            $new_ids = !empty($new_tags)
              ? _modularform_insert_new_tags($q_id, $new_tags, $now)
              : [];

            // Normalise new ids to strings for diff.
            $parts = array_map('strval', array_merge($existing_ids, $new_ids));
          }
          else {
            $parts = is_array($raw)
              ? array_values(array_filter($raw, fn($v) => $v !== '' && $v !== NULL && $v !== 0 && $v !== '0'))
              : array_filter(array_map('trim', explode(',', (string) $raw)), fn($v) => $v !== '');

            // Normalise to strings for diff.
            $parts = array_map('strval', $parts);
          }

          // Fetch existing values normalised to strings.
          $existing = db_select('modularform_answers_multi', 'am')
            ->fields('am', ['value'])
            ->condition('am.answer_id', $answer_id)
            ->execute()
            ->fetchCol();

          $existing = array_map('strval', $existing);

          $to_delete = array_diff($existing, $parts);
          $to_insert = array_diff($parts, $existing);

          if (!empty($to_delete)) {
            db_delete('modularform_answers_multi')
              ->condition('answer_id', $answer_id)
              ->condition('value', $to_delete, 'IN')
              ->execute();
          }

          foreach ($to_insert as $val) {
            db_insert('modularform_answers_multi')
              ->fields([
                'answer_id' => $answer_id,
                'value' => $val,
              ])
              ->execute();
          }
        }
      }
    }

    $response = entity_load_single('modularform_response', $response_id);
    $response->updated_at = $now;
    entity_save('modularform_response', $response);

    drupal_set_message(t('Response updated successfully.'));
    $form_state['redirect'] = 'forms/response/list';
  }
  catch (Exception $e) {
    $transaction->rollback();
    watchdog_exception('modularform', $e);
    drupal_set_message(t('An error occurred while saving. Please try again.'), 'error');
  }
}
/**
 * Maps a raw form value to the correct answer row columns based on question
 * type.
 *
 * @param string $q_type Question type.
 * @param mixed $raw Raw form value.
 *
 * @return array Partial answer row with the appropriate value column set.
 */
function _modularform_map_answer_value($q_type, $raw) {
  $result = [
    'value_varchar' => NULL,
    'value_text' => NULL,
    'value_int' => NULL,
    'value_bool' => NULL,
    'value_date' => NULL,
  ];

  switch ($q_type) {
    case 'text':
      $result['value_varchar'] = (string) $raw;
      break;

    case 'textarea':
    case 'richtext':
      $result['value_text'] = (string) $raw;
      break;

    case 'radio':
      $result['value_varchar'] = $raw;
      break;

    case 'boolean':
      // Accepts both raw checkbox (1/0) and radio ('yes'/'no') formats.
      $result['value_bool'] = ($raw === 'yes' || $raw == 1) ? 1 : 0;
      break;

    case 'date':
    case 'time':
      if (is_array($raw)) {
        $raw = $raw['date'] ?? $raw['value'] ?? implode(' ', array_filter($raw));
      }
      $raw = trim((string) $raw);
      $parsed = strtotime($raw);
      $result['value_date'] = ($raw !== '' && $parsed !== FALSE) ? $parsed : NULL;
      break;
  }

  return $result;
}

/**
 * Saves uploaded files for a file question and records them in answer_files.
 *
 * @param string $q_id Question ID.
 * @param bool $is_multiple Whether multiple files are allowed.
 * @param int $response_id Response ID for destination path.
 * @param int $answer_id Answer row ID to attach files to.
 * @param int $now Timestamp.
 */
function _modularform_save_answer_files($q_id, $is_multiple, $response_id, $answer_id, $now) {
  $upload_key = 'file_' . $q_id;
  $destination = 'private://modularform/responses/' . $response_id;
  file_prepare_directory($destination, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS);

  $validators = ['file_validate_size' => [25 * 1024 * 1024]];
  $uploads = [];

  if ($is_multiple) {
    $file_names = $_FILES['files']['name'][$upload_key] ?? [];
    $file_tmps = $_FILES['files']['tmp_name'][$upload_key] ?? [];
    $file_errors = $_FILES['files']['error'][$upload_key] ?? [];
    $file_sizes = $_FILES['files']['size'][$upload_key] ?? [];

    foreach ($file_names as $i => $filename) {
      if ($file_errors[$i] !== UPLOAD_ERR_OK || empty($filename)) {
        continue;
      }

      $filename = file_munge_filename(trim(drupal_basename($filename)), '');
      $uri = file_destination($destination . '/' . $filename, FILE_EXISTS_RENAME);

      if (!move_uploaded_file($file_tmps[$i], drupal_realpath($uri))) {
        watchdog('modularform', 'Failed to move uploaded file @file', ['@file' => $filename], WATCHDOG_ERROR);
        continue;
      }

      $file = new stdClass();
      $file->uid = $GLOBALS['user']->uid;
      $file->filename = $filename;
      $file->uri = $uri;
      $file->filemime = file_get_mimetype($filename);
      $file->filesize = $file_sizes[$i];
      $file->status = FILE_STATUS_PERMANENT;
      $file->timestamp = $now;

      file_save($file);
      $uploads[] = $file;
    }
  }
  else {
    $file = file_save_upload($upload_key, $validators, $destination, FILE_EXISTS_RENAME);
    if ($file) {
      $file->status = FILE_STATUS_PERMANENT;
      file_save($file);
      $uploads[] = $file;
    }
  }

  // Register usage — caller handles answer_files insert.
  foreach ($uploads as $file) {
    file_usage_add($file, 'modularform', 'response', $response_id);
  }

  return $uploads;
}

/**
 * @param $response_id
 *
 * loads default for edit
 *
 * @return array
 */
function _modularform_load_defaults($response_id) {
  $data = _modularform_load_response_data($response_id);
  $defaults = [];

  foreach ($data as $q_id => $entry) {
    $answer = $entry['answer'];

    switch ($entry['type']) {
      case 'radio':
      case 'text':
        $defaults[$q_id] = $answer->value_varchar;
        break;

      case 'list':
      case 'checkbox':
      case 'dropdown':
        $defaults[$q_id] = array_column($entry['multi'], 'id');
        break;

      case 'textarea':
      case 'richtext':
        $defaults[$q_id] = $answer->value_text;
        break;

      case 'boolean':
        $defaults[$q_id] = $answer->value_bool ? 'yes' : 'no';
        break;

      case 'date':
        $defaults[$q_id] = $answer->value_date
          ? date('Y-m-d', $answer->value_date)
          : NULL;
        break;

      case 'time':
        $defaults[$q_id] = $answer->value_date
          ? date('H:i', $answer->value_date)
          : NULL;
        break;

      case 'tag':
        $defaults[$q_id] = array_map(
          fn($m) => (object) ['id' => (string) $m['id'], 'text' => $m['label']],
          $entry['multi']
        );
        break;

      case 'file':
        // Can't pre-populate file inputs — pass existing files for display only.
        $defaults[$q_id] = $entry['files'];
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
