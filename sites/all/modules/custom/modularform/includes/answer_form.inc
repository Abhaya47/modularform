<?php

require_once dirname(__FILE__) . '/builder_build_fields.inc';
module_load_include('inc', 'modularform', 'modularform.rules');
module_load_include('inc', 'modularform', 'includes/load_response_data');

function modularform_answer_form($form, &$form_state, $fid) {
  $fid = (int) $fid;
  if (!$fid) {
    drupal_not_found();
    drupal_exit();
  }

  $form_record = _modularform_get_form($fid);
  if (!$form_record) {
    drupal_not_found();
    drupal_exit();
  }

  $elements = _modularform_load_elements($fid);
  _modularform_build_fields($form, $elements, $fid, 'submit');

  $form['#cache'] = TRUE;

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Submit'),
  ];
  return $form;
}

//helper function for file upload validation
function modularform_answer_form_submit($form, &$form_state) {
  global $user;
  $values = $form_state['values'];
  $fid = $form_state['build_info']['args'][0];
  $elements = _modularform_load_elements($fid);

  $now = REQUEST_TIME;
  $skip = ['submit', 'form_build_id', 'form_token', 'form_id', 'op'];

  $form_record = _modularform_get_form($fid);

  $is_anonymous_form = !empty($form_record->anonymous_responses);
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
  $datatype_map = modularform_datatype_map();
  $type_map = modularform_type_map();

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
          'datatype' => $type_map[$q_type],
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

        $uploaded = _modularform_save_answer_files($q_id, $is_multiple, $response_id, $elements);
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
  drupal_set_message('response saved');
  drupal_goto('/forms/manage');
}

function _modularform_validate_file_question($q_id, array $elements) {
  $file_rules = _modularform_file_validators($q_id, $elements);
  $validators = $file_rules['drupal'];
  $min_files  = $file_rules['min_files'];
  $max_files  = $file_rules['max_files'];
  $upload_key = 'file_' . $q_id;

  $file_names = $_FILES['files']['name'][$upload_key] ?? [];
  $file_names = is_array($file_names) ? $file_names : [$file_names];
  $file_names = array_filter($file_names, fn($n) => $n !== '');
  $count      = count($file_names);

  if ($min_files !== NULL && $count < $min_files) {
    form_set_error('files][' . $upload_key, t('Upload at least @min file(s).', ['@min' => $min_files]));
    return;
  }
  if ($max_files !== NULL && $count > $max_files) {
    form_set_error('files][' . $upload_key, t('Upload at most @max file(s).', ['@max' => $max_files]));
    return;
  }

  if (isset($validators['file_validate_size'])) {
    $max_bytes  = $validators['file_validate_size'][0];
    $file_sizes = $_FILES['files']['size'][$upload_key] ?? [];
    $file_sizes = is_array($file_sizes) ? $file_sizes : [$file_sizes];
    foreach ($file_sizes as $size) {
      if ((int) $size > $max_bytes) {
        form_set_error('files][' . $upload_key, t('File exceeds maximum size of @mb MB.', ['@mb' => $max_bytes / 1024 / 1024]));
        return;
      }
    }
  }

  if (isset($validators['file_validate_extensions'])) {
    $allowed = explode(' ', $validators['file_validate_extensions'][0]);
    foreach ($file_names as $name) {
      $ext = strtolower(pathinfo($name, PATHINFO_EXTENSION));
      if (!in_array($ext, $allowed)) {
        form_set_error('files][' . $upload_key, t('File type .@ext is not allowed.', ['@ext' => $ext]));
        return;
      }
    }
  }
}

function modularform_answer_form_validate($form, &$form_state) {
  $fid = $form_state['build_info']['args'][0];
  $elements = _modularform_load_elements($fid);
  $values = $form_state['values'];

  $questions = array_filter($elements, fn($el) => $el['type'] === 'question');
  $sections = array_filter($elements, fn($el) => $el['type'] === 'section');

  // Group rules by parent question ID.
  $rules_by_question = [];
  foreach ($elements as $el) {
    if ($el['type'] === 'validation_rule' && $el['rule_type'] !== 'is_multiple') {
      $rules_by_question[$el['parent']][] = $el;
    }
  }

  foreach ($sections as $sec_id => $sec) {
    foreach ($questions as $q_id => $q) {
      if ($q['parent'] !== $sec_id) {
        continue;
      }

      $q_type = $q['question_type'];
      $rules = $rules_by_question[$q_id] ?? [];
      $raw = $values[$sec_id][$q_id]['answer'] ?? NULL;

      if ($q_type === 'file') {
        _modularform_validate_file_question($q_id, $elements);
        continue;
      }

      // Required check.
      if (!empty($q['is_required'])) {
        $empty = FALSE;
        if (is_array($raw)) {
          $filtered = array_filter($raw, fn($v) => $v !== '' && $v !== NULL && $v !== 0 && $v !== FALSE && $v !== '0');
          $empty = empty($filtered);
        }
        else {
          $empty = ($raw === NULL || $raw === '');
        }
        if ($empty) {
          form_set_error(
            $sec_id . '][' . $q_id . '][answer',
            t('This field is required.')
          );
          continue; // no point running rules on empty required field
        }
      }

      // Skip rule validation if no rules or value is empty.
      if (empty($rules) || $raw === NULL || $raw === '') {
        continue;
      }

      foreach ($rules as $rule) {
        $error = _modularform_validate_answer($q_type, $raw, $rule);
        if ($error !== NULL) {
          form_set_error($sec_id . '][' . $q_id . '][answer', $error);
          break; // one error per question is enough
        }
      }
    }
  }
}

function _modularform_validate_answer($q_type, $raw, $rule) {
  $rule_type = $rule['rule_type'];
  $rule_value = $rule['rule_value'];
  $custom_msg = !empty($rule['error_message']) ? $rule['error_message'] : NULL;

  $err = fn($default) => $custom_msg ?? t($default);

  switch ($q_type) {
    case 'text':
      $val = (string) $raw;
      $len = mb_strlen($val);
      switch ($rule_type) {
        case 'min_length':
          return $len < (int) $rule_value ? $err('Must be at least ' . (int) $rule_value . ' characters.') : NULL;
        case 'max_length':
          return $len > (int) $rule_value ? $err('Must be at most ' . (int) $rule_value . ' characters.') : NULL;
        case 'exact_length':
          return $len !== (int) $rule_value ? $err('Must be exactly ' . (int) $rule_value . ' characters.') : NULL;
        case 'is_email':
          return !filter_var($val, FILTER_VALIDATE_EMAIL) ? $err('Must be a valid email address.') : NULL;
        case 'is_url':
          return !filter_var($val, FILTER_VALIDATE_URL) ? $err('Must be a valid URL.') : NULL;
        case 'is_numeric':
          return !is_numeric($val) ? $err('Must contain numbers only.') : NULL;
        case 'is_alpha':
          return !ctype_alpha($val) ? $err('Must contain letters only.') : NULL;
        case 'starts_with':
          return strpos($val, $rule_value) !== 0 ? $err('Must start with "' . $rule_value . '".') : NULL;
        case 'ends_with':
          return substr($val, -mb_strlen($rule_value)) !== $rule_value ? $err('Must end with "' . $rule_value . '".') : NULL;
        case 'contains':
          return strpos($val, $rule_value) === FALSE ? $err('Must contain "' . $rule_value . '".') : NULL;
        case 'regex':
          return !preg_match($rule_value, $val) ? $err('Invalid format.') : NULL;
      }
      break;

    case 'textarea':
      $val = (string) $raw;
      $len = mb_strlen($val);
      $words = str_word_count(strip_tags($val));
      switch ($rule_type) {
        case 'min_length':
          return $len < (int) $rule_value ? $err('Must be at least ' . (int) $rule_value . ' characters.') : NULL;
        case 'max_length':
          return $len > (int) $rule_value ? $err('Must be at most ' . (int) $rule_value . ' characters.') : NULL;
        case 'min_words':
          return $words < (int) $rule_value ? $err('Must contain at least ' . (int) $rule_value . ' words.') : NULL;
        case 'max_words':
          return $words > (int) $rule_value ? $err('Must contain at most ' . (int) $rule_value . ' words.') : NULL;
        case 'contains':
          return strpos($val, $rule_value) === FALSE ? $err('Must contain "' . $rule_value . '".') : NULL;
        case 'regex':
          return !preg_match($rule_value, $val) ? $err('Invalid format.') : NULL;
      }
      break;

    case 'richtext':
      $plain = strip_tags((string) $raw);
      $words = str_word_count($plain);
      switch ($rule_type) {
        case 'min_words':
          return $words < (int) $rule_value ? $err('Must contain at least ' . (int) $rule_value . ' words.') : NULL;
        case 'max_words':
          return $words > (int) $rule_value ? $err('Must contain at most ' . (int) $rule_value . ' words.') : NULL;
        case 'contains':
          return strpos($plain, $rule_value) === FALSE ? $err('Must contain "' . $rule_value . '".') : NULL;
      }
      break;

    case 'date':
      $raw_str = trim((string) $raw);
      $d = DateTime::createFromFormat('Y-m-d', $raw_str);
      if (!$d || $d->format('Y-m-d') !== $raw_str) {
        return $err('Invalid date. Use YYYY-MM-DD format.');
      }
      $parsed = $d->getTimestamp();
      switch ($rule_type) {
        case 'date_min':
          $min = DateTime::createFromFormat('Y-m-d', $rule_value);
          return ($min && $parsed < $min->getTimestamp()) ? $err('Date must be on or after ' . $rule_value . '.') : NULL;
        case 'date_max':
          $max = DateTime::createFromFormat('Y-m-d', $rule_value);
          return ($max && $parsed > $max->getTimestamp()) ? $err('Date must be on or before ' . $rule_value . '.') : NULL;
        case 'date_format':
          $d2 = DateTime::createFromFormat($rule_value, $raw_str);
          return (!$d2 || $d2->format($rule_value) !== $raw_str) ? $err('Date must be in format: ' . $rule_value . '.') : NULL;
      }
      break;

    case 'time':
      // Expect HH:MM
      if (!preg_match('/^\d{2}:\d{2}$/', (string) $raw)) {
        return $err('Invalid time format. Use HH:MM.');
      }
      switch ($rule_type) {
        case 'time_min':
          return (strcmp((string) $raw, $rule_value) < 0) ? $err('Time must be at or after ' . $rule_value . '.') : NULL;
        case 'time_max':
          return (strcmp((string) $raw, $rule_value) > 0) ? $err('Time must be at or before ' . $rule_value . '.') : NULL;
      }
      break;

    case 'boolean':
      switch ($rule_type) {
        case 'must_be_true':
          return ($raw !== 'yes' && $raw != 1) ? $err('Must be Yes.') : NULL;
        case 'must_be_false':
          return ($raw !== 'no' && $raw != 0) ? $err('Must be No.') : NULL;
      }
      break;

    case 'radio':
      switch ($rule_type) {
        case 'disallow_value':
          return ((string) $raw === (string) $rule_value) ? $err('"' . $rule_value . '" is not allowed.') : NULL;
      }
      break;

    case 'list':
    case 'checkbox':
      $selected = is_array($raw)
        ? array_filter($raw, fn($v) => $v !== 0 && $v !== FALSE && $v !== '0')
        : [];
      $count = count($selected);
      switch ($rule_type) {
        case 'min_selections':
          return $count < (int) $rule_value ? $err('Select at least ' . (int) $rule_value . ' option(s).') : NULL;
        case 'max_selections':
          return $count > (int) $rule_value ? $err('Select at most ' . (int) $rule_value . ' option(s).') : NULL;
        case 'exact_count':
          return $count !== (int) $rule_value ? $err('Select exactly ' . (int) $rule_value . ' option(s).') : NULL;
        case 'disallow_value':
          return isset($selected[$rule_value]) ? $err('"' . $rule_value . '" is not allowed.') : NULL;
      }
      break;

    case 'dropdown':
      $selected = is_array($raw) ? array_filter($raw, fn($v) => $v !== '' && $v !== NULL) : [$raw];
      $count = count($selected);
      switch ($rule_type) {
        case 'min_selections':
          return $count < (int) $rule_value ? $err('Select at least ' . (int) $rule_value . ' option(s).') : NULL;
        case 'max_selections':
          return $count > (int) $rule_value ? $err('Select at most ' . (int) $rule_value . ' option(s).') : NULL;
        case 'disallow_value':
          return in_array((string) $rule_value, array_map('strval', $selected)) ? $err('"' . $rule_value . '" is not allowed.') : NULL;
      }
      break;

    case 'tag':
      // Raw is a comma-separated string of IDs and/or "new:label" entries.
      $parts = is_array($raw)
        ? array_filter($raw, fn($v) => $v !== '' && $v !== NULL)
        : array_filter(array_map('trim', explode(',', (string) $raw)), fn($v) => $v !== '');
      $count = count($parts);
      switch ($rule_type) {
        case 'min_tags':
          return $count < (int) $rule_value ? $err('Add at least ' . (int) $rule_value . ' tag(s).') : NULL;
        case 'max_tags':
          return $count > (int) $rule_value ? $err('Add at most ' . (int) $rule_value . ' tag(s).') : NULL;
        case 'exact_tags':
          return $count !== (int) $rule_value ? $err('Add exactly ' . (int) $rule_value . ' tag(s).') : NULL;
      }
      break;
  }

  return NULL;
}

function modularform_response_edit_form($form, &$form_state, $response_id) {
  $response_id = (int) $response_id;
  if (!$response_id) {
    drupal_not_found();
    drupal_exit();
  }

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


function modularform_response_edit_form_validate($form, &$form_state) {
  $fid = $form_state['storage']['form_id'];
  $elements = _modularform_load_elements($fid);
  $values = $form_state['values'];

  $questions = array_filter($elements, fn($el) => $el['type'] === 'question');
  $sections = array_filter($elements, fn($el) => $el['type'] === 'section');

  $rules_by_question = [];
  foreach ($elements as $el) {
    if ($el['type'] === 'validation_rule' && $el['rule_type'] !== 'is_multiple') {
      $rules_by_question[$el['parent']][] = $el;
    }
  }

  foreach ($sections as $sec_id => $sec) {
    foreach ($questions as $q_id => $q) {
      if ($q['parent'] !== $sec_id) {
        continue;
      }

      $q_type = $q['question_type'];
      $rules = $rules_by_question[$q_id] ?? [];
      $raw = $values[$sec_id][$q_id]['answer'] ?? NULL;

      if ($q_type === 'file') {
        _modularform_validate_file_question($q_id, $elements);
        continue;
      }
      // In edit mode empty = "unchanged", so we only validate if something
      // was actually submitted. Required is not re-enforced on edit since
      // the answer already exists in the DB.
      if ($raw === NULL || $raw === '') {
        continue;
      }

      if (empty($rules)) {
        continue;
      }

      foreach ($rules as $rule) {
        $error = _modularform_validate_answer($q_type, $raw, $rule);
        if ($error !== NULL) {
          form_set_error($sec_id . '][' . $q_id . '][answer', $error);
          break;
        }
      }
    }
  }
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

            $uploaded = _modularform_save_answer_files($q_id, $is_multiple, $response_id, $elements);

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
function _modularform_save_answer_files($q_id, $is_multiple, $response_id, $elements) {
  $upload_key = 'file_' . $q_id;
  $destination = 'private://modularform/responses/' . (int) $response_id;
  $file_rules = _modularform_file_validators($q_id, $elements); // needs $elements passed in
  $validators = $file_rules['drupal'];
  $min_files = $file_rules['min_files'];
  $max_files = $file_rules['max_files'];
  $uploads = [];

  file_prepare_directory($destination, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS);

  if ($is_multiple) {
    $file_names = $_FILES['files']['name'][$upload_key] ?? [];
    $file_tmps = $_FILES['files']['tmp_name'][$upload_key] ?? [];
    $file_errors = $_FILES['files']['error'][$upload_key] ?? [];
    $file_types = $_FILES['files']['type'][$upload_key] ?? [];
    $file_sizes = $_FILES['files']['size'][$upload_key] ?? [];

    foreach ($file_names as $i => $filename) {
      if ($file_errors[$i] !== UPLOAD_ERR_OK || empty($filename)) {
        continue;
      }

      try {
        $_FILES['files']['name'][$upload_key] = $filename;
        $_FILES['files']['tmp_name'][$upload_key] = $file_tmps[$i];
        $_FILES['files']['error'][$upload_key] = $file_errors[$i];
        $_FILES['files']['type'][$upload_key] = $file_types[$i];
        $_FILES['files']['size'][$upload_key] = $file_sizes[$i];

        $file = file_save_upload($upload_key, $validators, $destination, FILE_EXISTS_RENAME);
        if ($file) {
          $file->status = FILE_STATUS_PERMANENT;
          file_save($file);
          $uploads[] = $file;
        }
      }
      finally {
        // Always restore original structure even if file_save_upload throws
        $_FILES['files']['name'][$upload_key] = $file_names;
        $_FILES['files']['tmp_name'][$upload_key] = $file_tmps;
        $_FILES['files']['error'][$upload_key] = $file_errors;
        $_FILES['files']['type'][$upload_key] = $file_types;
        $_FILES['files']['size'][$upload_key] = $file_sizes;
      }
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

  $count = count($uploads);

  if ($min_files !== NULL && $count < $min_files) {
    form_set_error('file_' . $q_id, t('Upload at least @min file(s).', ['@min' => $min_files]));
    // clean up any files already saved
    foreach ($uploads as $file) {
      file_delete($file);
    }
    return [];
  }

  if ($max_files !== NULL && $count > $max_files) {
    form_set_error('file_' . $q_id, t('Upload at most @max file(s).', ['@max' => $max_files]));
    foreach ($uploads as $file) {
      file_delete($file);
    }
    return [];
  }

  foreach ($uploads as $file) {
    file_usage_add($file, 'modularform', 'response', $response_id);
  }

  return $uploads;
}

/**
 * Builds the file validators array for a given question by reading its
 * stored validation rules from the DB.
 *
 * Maps rule_type values:
 *   max_filesize  → file_validate_size (converts MB to bytes)
 *   allowed_types → file_validate_extensions (expands group to extensions)
 *
 * Always adds file_validate_name_length as a safe default.
 *
 * @param int $q_id
 *
 * @return array Drupal file validators array.
 */
function _modularform_file_validators($q_id, array $elements) {
  $type_map = [
    'image' => 'jpg jpeg png gif webp',
    'document' => 'pdf doc docx odt',
    'video' => 'mp4 mov avi',
    'audio' => 'mp3 wav ogg',
    'archive' => 'zip tar gz',
  ];

  $rules = [];
  foreach ($elements as $el) {
    if ($el['type'] === 'validation_rule'
      && $el['parent'] === $q_id
      && in_array($el['rule_type'], [
        'max_filesize',
        'allowed_types',
        'min_files',
        'max_files',
      ])
    ) {
      $rules[$el['rule_type']] = $el['rule_value'];
    }
  }

  $validators = [];

  if (isset($rules['max_filesize']) && is_numeric($rules['max_filesize'])) {
    $validators['file_validate_size'] = [(int) $rules['max_filesize'] * 1024 * 1024];
  }
  else {
    $validators['file_validate_size'] = [5 * 1024 * 1024];
  }

  if (isset($rules['allowed_types'])) {
    $group = $rules['allowed_types'];
    if ($group !== 'any' && isset($type_map[$group])) {
      $validators['file_validate_extensions'] = [$type_map[$group]];
    }
  }
  else {
    $validators['file_validate_extensions'] = ['pdf doc docx png jpg jpeg gif zip txt'];
  }

  $validators['file_validate_name_length'] = [];

  return [
    'drupal' => $validators,
    'min_files' => isset($rules['min_files']) ? (int) $rules['min_files'] : NULL,
    'max_files' => isset($rules['max_files']) ? (int) $rules['max_files'] : NULL,
  ];
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
