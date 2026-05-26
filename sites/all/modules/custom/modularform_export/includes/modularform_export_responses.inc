<?php

/**
 * @file
 * Batch operation callbacks for modularform_export response exports.
 *
 * One CSV per form. Columns: email, then one column per question (label).
 * Each row is one response. Multi-value answers (checkbox, tag) are
 * comma-joined in a single cell. Single-select answers use the option label.
 */

/**
 * Batch operation: export all responses for a single form to CSV.
 *
 * @param int $fid
 *   The form ID to process.
 * @param array $context
 *   Batch context.
 */
function _modularform_export_responses_process_form($fid, &$context) {
  if (!modularform_export_load_openspout()) {
    $context['results']['errors'][] = t('OpenSpout library not available for form @fid.', ['@fid' => $fid]);
    return;
  }

  // 1. Load ordered questions for this form so we can build a stable header.
  $questions = _modularform_export_responses_load_questions($fid);

  if (empty($questions)) {
    watchdog('modularform_export', 'No questions found for form @fid, skipping responses export.', ['@fid' => $fid], WATCHDOG_WARNING);
    return;
  }

  // 2. Load all response IDs for this form.
  $response_ids = db_select('modularform_responses', 'r')
    ->fields('r', ['id', 'email'])
    ->condition('r.form_id', $fid)
    ->orderBy('r.id', 'ASC')
    ->execute()
    ->fetchAllAssoc('id');

  if (empty($response_ids)) {
    watchdog('modularform_export', 'No responses found for form @fid, skipping.', ['@fid' => $fid], WATCHDOG_WARNING);
    return;
  }

  // 3. Build header: email + one column per question label.
  $header = ['response_id', 'email'];
  foreach ($questions as $q_id => $question) {
    $header[] = $question['label'];
  }

  // 4. Prepare output file.
  $real_dir = drupal_realpath('private://modularform_exports');
  $real_filepath = $real_dir . '/responses_form_' . $fid . '.csv';

  try {
    $writer = new \OpenSpout\Writer\CSV\Writer();
    $writer->openToFile($real_filepath);
    $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($header));

    // 5. One row per response.
    foreach ($response_ids as $response_id => $response_meta) {
      $email = !empty($response_meta->email) ? $response_meta->email : t('anonymous');

      // _modularform_load_response_data() returns array keyed by q_id.
      $answer_data = _modularform_load_response_data($response_id);

      $row = [(int) $response_id, $email];
      foreach ($questions as $q_id => $question) {
        $row[] = _modularform_export_responses_format_answer(
          isset($answer_data[$q_id]) ? $answer_data[$q_id] : NULL
        );
      }

      $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($row));
    }

    $writer->close();

    $context['results']['files'][] = $real_filepath;
    $context['message'] = t('Exported responses for form @fid.', ['@fid' => $fid]);
  }
  catch (Exception $e) {
    watchdog('modularform_export', 'Failed to write responses CSV for form @fid: @msg', [
      '@fid' => $fid,
      '@msg' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    $context['results']['errors'][] = t('Failed to export responses for form @fid.', ['@fid' => $fid]);
  }
}

/**
 * Loads ordered questions for a form, keyed by q_id.
 *
 * Joins sections so we can sort by section position then question position,
 * giving the same column order a respondent sees in the form.
 *
 * @param int $fid
 *   The form ID.
 *
 * @return array
 *   Associative array keyed by q_id, each value:
 *   ['label' => string, 'question_type' => string]
 *   Ordered by section weight, question weight.
 */
function _modularform_export_responses_load_questions($fid) {
  // _modularform_load_elements() returns all elements (sections, questions,
  // options, rules) for a form. We only need questions here, ordered properly.
  $elements = _modularform_load_elements($fid);

  if (empty($elements)) {
    return [];
  }

  // Separate sections and questions.
  $sections  = [];
  $questions = [];
  foreach ($elements as $id => $el) {
    if ($el['type'] === 'section') {
      $sections[$id] = $el;
    }
    elseif ($el['type'] === 'question') {
      $questions[$id] = $el;
    }
  }

  // Sort questions by section weight, then question weight.
  uasort($questions, function ($a, $b) use ($sections) {
    $sec_a = isset($sections[$a['parent']]) ? (int) $sections[$a['parent']]['weight'] : 0;
    $sec_b = isset($sections[$b['parent']]) ? (int) $sections[$b['parent']]['weight'] : 0;

    if ($sec_a !== $sec_b) {
      return $sec_a - $sec_b;
    }

    return (int) $a['position'] - (int) $b['position'];
  });

  // Return only what we need for header building and cell lookup.
  $result = [];
  foreach ($questions as $q_id => $q) {
    $result[$q_id] = [
      'label'         => $q['label'],
      'question_type' => $q['question_type'],
    ];
  }

  return $result;
}

/**
 * Formats a single answer entry from _modularform_load_response_data() into
 * a plain string suitable for a CSV cell.
 *
 * Multi-value answers (checkbox, tag, dropdown) are comma-joined using their labels.
 * Single-select answers (radio) use the resolved option label.
 * Text-like answers use the raw stored value.
 * Unanswered questions produce an empty string.
 *
 * @param array|null $answer_entry
 *   A single entry from the array returned by _modularform_load_response_data(),
 *   or NULL if the question was not answered.
 *
 * @return string
 *   The formatted cell value.
 */
function _modularform_export_responses_format_answer($answer_entry) {
  if (empty($answer_entry)) {
    return '';
  }

  $type         = $answer_entry['type'];
  $answer       = $answer_entry['answer'];
  $datatype_map = modularform_datatype_map();

  // Multi-value types — keyed off datatype_map, not hardcoded type names.
  if (($datatype_map[$type] ?? NULL) === 'multi') {
    $labels = array_column($answer_entry['multi'] ?? [], 'label');
    return implode(', ', $labels);
  }

  // Single-select: radio — use the resolved option label.
  if ($type === 'radio') {
    return $answer->option_label ?? $answer->value_varchar ?? '';
  }

  // Date type.
  if ($type === 'date' && !empty($answer->value_date)) {
    return date('Y-m-d', $answer->value_date);
  }

  // Time type.
  if ($type === 'time' && !empty($answer->value_date)) {
    return date('H:i', $answer->value_date);
  }

  // Numeric type.
  if ($type === 'number' && isset($answer->value_int)) {
    return (string) $answer->value_int;
  }

  // Text, textarea, richtext, and any other scalar types.
  if (isset($answer->value_varchar) && $answer->value_varchar !== '') {
    return $answer->value_varchar;
  }
  if (isset($answer->value_text) && $answer->value_text !== '') {
    return $answer->value_text;
  }

  return '';
}
