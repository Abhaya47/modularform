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

  global $user;

  // Load the form record to check anonymous_responses is true or false
  $form_record = db_select('modularform_forms', 'f')
    ->fields('f', ['anonymous_responses'])
    ->condition('f.id', $fid)
    ->execute()
    ->fetchAssoc();

  $is_anonymous_form = !empty($form_record['anonymous_responses']);

  // Determine email to store
  if ($is_anonymous_form) {
    // Anonymous form — never store email
    $email = NULL;
  } elseif ($user->uid) {
    // Logged-in user on non-anonymous form — use their account email
    $email = $user->mail;
  } else {
    // Guest user on non-anonymous form — use submitted email field
    $email = isset($form_state['values']['_guest_email'])
      ? trim($form_state['values']['_guest_email'])
      : '';
  }

  $fields = [
    'form_id'      => $fid,
    'user_id'      => $user->uid,
    'submitted_at' => $now,
    'created_at'   => $now,
    'updated_at'   => $now,
  ];

  // Only include email column when we have a value or it's a non-anonymous form
  // (NULL is valid for anonymous; omitting lets the DB default handle it)
  if (!$is_anonymous_form) {
    $fields['email'] = $email;
  }

  $response_id = db_insert('modularform_responses')
    ->fields($fields)
    ->execute();

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
      $datatype_str = modularform_get_datatype($q_type);
      $datatype_int = modularform_get_datatype_int_map($datatype_str);
      $is_multi = in_array($q_type, $multi_types);

      // --- 3. Build answer row ---
      $answer_row = [
        'response_id' => $response_id,
        'q_id' => $q_id,
        'datatype' => $datatype_int,
        'value_varchar' => NULL,
        'value_text' => NULL,
        'value_int' => NULL,
        'value_bool' => NULL,
        'value_date' => NULL,
        'created_at' => $now,
        'updated_at' => $now,
      ];

      if ($is_multi) {
        // multi — value_int stays NULL, child rows handle it
        // parse and insert after we have answer_id
      }
      else {
        switch ($datatype_str) {
          case 'varchar':
            $answer_row['value_varchar'] = (string) $raw;
            break;
          case 'text':
            $answer_row['value_text'] = (string) $raw;
            break;
          case 'int':
            // radio/dropdown — raw is the option natural key string e.g. 'b'
            // resolve to o_id int would go here if surrogate PKs
            // for now store as-is via value_varchar until PKs are refactored
            $answer_row['value_varchar'] = (string) $raw;
            break;
          case 'bool':
            $answer_row['value_bool'] = $raw ? 1 : 0;
            break;
          case 'date':
            $answer_row['value_date'] = !empty($raw) ? strtotime($raw) : NULL;
            break;
        }
      }

      $answer_id = db_insert('modularform_answers')
        ->fields($answer_row)
        ->execute();

      // --- 4. Multi-value: parse and insert child rows ---
      if ($is_multi && !empty($raw)) {
        $parts = explode(',', $raw);

        foreach ($parts as $part) {
          $part = trim($part);

          if (strpos($part, 'new:') === 0) {
            // new tag — insert into options table first, get back o_id
            $tag_label = substr($part, 4);
            $new_o_id = _modularform_insert_new_tag($q_id, $tag_label, $now);
            $resolved = $new_o_id;
          }
          else {
            // existing option natural key — store as-is for now
            $resolved = (int) $part;
          }

          db_insert('modularform_answers_multi')
            ->fields([
              'answer_id' => $answer_id,
              'value' => $resolved,
            ])
            ->execute();
        }
      }
    }
  }
}
