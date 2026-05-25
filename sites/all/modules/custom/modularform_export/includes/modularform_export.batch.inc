<?php

/**
 * @file
 * Batch operation callbacks for modularform_export.
 */

/**
 * Batch operation: load and export a single form to CSV.
 *
 * @param int $fid
 *   The form ID to process.
 * @param array $context
 *   Batch context.
 */
function _modularform_export_process_form($fid, &$context) {
  if (!modularform_export_load_openspout()) {
    $context['results']['errors'][] = t('OpenSpout library not available for form @fid.', ['@fid' => $fid]);
    return;
  }

  $elements = _modularform_load_elements($fid);

  if (empty($elements)) {
    watchdog('modularform_export', 'No elements found for form @fid, skipping.', ['@fid' => $fid], WATCHDOG_WARNING);
    return;
  }

  $rows = _modularform_export_flatten($fid, $elements);

  if (empty($rows)) {
    watchdog('modularform_export', 'No rows generated for form @fid, skipping.', ['@fid' => $fid], WATCHDOG_WARNING);
    return;
  }

  $filepath = 'private://modularform_exports/form_' . $fid . '.csv';
  $real_filepath = drupal_realpath('private://modularform_exports') . '/form_' . $fid . '.csv';

  try {
    $writer = new \OpenSpout\Writer\CSV\Writer();
    $writer->openToFile($real_filepath);

    // Write header row on first form only — or per file since one CSV per form.
    $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues(_modularform_export_csv_headers()));

    foreach ($rows as $row) {
      $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($row));
    }

    $writer->close();

    $context['results']['files'][] = $real_filepath;
    $context['message'] = t('Exported form @fid.', ['@fid' => $fid]);
  }
  catch (Exception $e) {
    watchdog('modularform_export', 'Failed to write CSV for form @fid: @msg', [
      '@fid' => $fid,
      '@msg' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    $context['results']['errors'][] = t('Failed to export form @fid.', ['@fid' => $fid]);
  }
}

/**
 * Returns the CSV header row.
 *
 * @return array
 *   Ordered array of column header strings.
 */
function _modularform_export_csv_headers() {
  return [
    'form_id',
    'section_id',
    'section_title',
    'section_weight',
    'question_id',
    'question_label',
    'question_type',
    'is_required',
    'option_id',
    'option_label',
    'option_weight',
    'rule_id',
    'rule_type',
    'rule_value',
    'error_message',
  ];
}

/**
 * Flattens the elements array into denormalized CSV rows.
 *
 * Each row is a leaf node (option or validation rule) with all
 * ancestor data repeated inline. Questions with no children get
 * one row with empty option/rule columns.
 *
 * @param int $fid
 *   The form ID.
 * @param array $elements
 *   Elements array from _modularform_load_elements().
 *
 * @return array
 *   Array of flat row arrays ready for CSV output.
 */
function _modularform_export_flatten($fid, array $elements) {
  // Bucket elements by type for easier traversal.
  $sections   = [];
  $questions  = [];
  $options    = [];
  $rules      = [];

  foreach ($elements as $id => $el) {
    switch ($el['type']) {
      case 'section':
        $sections[$id] = $el;
        break;
      case 'question':
        $questions[$id] = $el;
        break;
      case 'option':
        $options[$id] = $el;
        break;
      case 'validation_rule':
        $rules[$id] = $el;
        break;
    }
  }

  // Index options and rules by parent question ID.
  $options_by_question = [];
  foreach ($options as $o_id => $option) {
    $options_by_question[$option['parent']][$o_id] = $option;
  }

  $rules_by_question = [];
  foreach ($rules as $r_id => $rule) {
    $rules_by_question[$rule['parent']][$r_id] = $rule;
  }

  $rows = [];

  foreach ($questions as $q_id => $question) {
    $sec_id  = $question['parent'];
    $section = isset($sections[$sec_id]) ? $sections[$sec_id] : [];

    $base = [
      'form_id'        => $fid,
      'section_id'     => $sec_id,
      'section_title'  => isset($section['title']) ? $section['title'] : '',
      'section_weight' => isset($section['weight']) ? $section['weight'] : '',
      'question_id'    => $q_id,
      'question_label' => $question['label'],
      'question_type'  => $question['question_type'],
      'is_required'    => $question['is_required'] ? '1' : '0',
    ];

    $q_options = isset($options_by_question[$q_id]) ? $options_by_question[$q_id] : [];
    $q_rules   = isset($rules_by_question[$q_id]) ? $rules_by_question[$q_id] : [];

    if (empty($q_options) && empty($q_rules)) {
      // Question with no children — one row, empty option/rule columns.
      $rows[] = array_values($base + [
        'option_id'     => '',
        'option_label'  => '',
        'option_weight' => '',
        'rule_id'       => '',
        'rule_type'     => '',
        'rule_value'    => '',
        'error_message' => '',
      ]);
      continue;
    }

    // One row per option.
    foreach ($q_options as $o_id => $option) {
      $rows[] = array_values($base + [
        'option_id'     => $o_id,
        'option_label'  => $option['option'],
        'option_weight' => $option['weight'],
        'rule_id'       => '',
        'rule_type'     => '',
        'rule_value'    => '',
        'error_message' => '',
      ]);
    }

    // One row per validation rule.
    foreach ($q_rules as $r_id => $rule) {
      $rows[] = array_values($base + [
        'option_id'     => '',
        'option_label'  => '',
        'option_weight' => '',
        'rule_id'       => $r_id,
        'rule_type'     => $rule['rule_type'],
        'rule_value'    => $rule['rule_value'],
        'error_message' => $rule['error_message'],
      ]);
    }
  }
  return $rows;
}
