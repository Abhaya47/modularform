<?php

/**
 * Load all elements for a form from DB into the storage flat-list format.
 * Returns empty array if form has no saved elements (create flow).
 */
function _modularform_load_elements($fid) {
  $elements = [];

  $type_map_flip = [
    0 => 'text',
    1 => 'textarea',
    2 => 'radio',
    3 => 'boolean',
    4 => 'checkbox',
    5 => 'file',
    6 => 'dropdown',
    7 => 'tag',
    8 => 'richtext',
    9 => 'date',
    10 => 'time',
    11 => 'list',
  ];

  // Sections
  $sections = db_select('modularform_sections', 's')
    ->fields('s', ['sec_id', 'name', 'weight'])
    ->condition('form_id', $fid)
    ->orderBy('weight')
    ->execute()
    ->fetchAllAssoc('sec_id', PDO::FETCH_ASSOC);

  if (empty($sections)) {
    return [];
  }

  foreach ($sections as $sec_id => $row) {
    $elements[$sec_id] = [
      'type' => 'section',
      'title' => $row['name'],
      'weight' => (int) $row['weight'],
      'parent' => NULL,
    ];
  }

  // Questions
  $questions = db_select('modularform_questions', 'q')
    ->fields('q', [
      'q_id',
      'sec_id',
      'label',
      'type',
      'is_required',
      'position',
    ])
    ->condition('form_id', $fid)
    ->orderBy('sec_id')
    ->orderBy('position')
    ->execute()
    ->fetchAllAssoc('q_id', PDO::FETCH_ASSOC);

  foreach ($questions as $q_id => $row) {
    $elements[$q_id] = [
      'type' => 'question',
      'parent' => $row['sec_id'],
      'label' => $row['label'],
      'question_type' => $type_map_flip[$row['type']] ?? 'text',
      'is_required' => (bool) $row['is_required'],
      'add_validation' => FALSE,  // overridden below if rules exist
      'weight' => (int) $row['position'],
    ];
  }

  // Options
  $options = db_select('modularform_question_options', 'o')
    ->fields('o', ['o_id', 'q_id', 'option', 'weight'])
    ->condition('form_id', $fid)
    ->orderBy('q_id')
    ->orderBy('weight')
    ->execute()
    ->fetchAllAssoc('o_id', PDO::FETCH_ASSOC);

  foreach ($options as $o_id => $row) {
    $elements[$o_id] = [
      'type' => 'option',
      'parent' => $row['q_id'],
      'option' => $row['option'],
      'weight' => (int) $row['weight'],
    ];
  }

  // Validation rules
  $rules = db_select('modularform_validation_rules', 'r')
    ->fields('r', [
      'r_id',
      'q_id',
      'rule_type',
      'rule_value',
      'error_message',
      'weight',
    ])
    ->condition('form_id', $fid)
    ->orderBy('q_id')
    ->orderBy('weight')
    ->execute()
    ->fetchAllAssoc('r_id', PDO::FETCH_ASSOC);

  foreach ($rules as $r_id => $row) {
    // Flag the parent question so the rule-builder renders open
    if (isset($elements[$row['q_id']])) {
      $elements[$row['q_id']]['add_validation'] = TRUE;
    }
    $elements[$r_id] = [
      'type' => 'validation_rule',
      'parent' => $row['q_id'],
      'rule_type' => $row['rule_type'],
      'rule_value' => $row['rule_value'],
      'error_message' => $row['error_message'],
      'weight' => (int) $row['weight'],
    ];
  }

  return $elements;
}
