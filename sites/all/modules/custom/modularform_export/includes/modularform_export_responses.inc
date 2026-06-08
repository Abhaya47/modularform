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
 * Batch operation: export responses for a chunk of forms to CSV.
 *
 * Loads all elements, response metadata, and answer data for the entire
 * chunk in a fixed number of queries, then writes one CSV file per form.
 *
 * @param int[] $fids
 *   Array of form IDs to process in this chunk.
 * @param array $context
 *   Batch API context. On completion, appends to:
 *   - $context['results']['files']: real filepaths of written CSVs.
 *   - $context['results']['errors']: translated error strings for any failures.
 */
function _modularform_export_responses_process_chunk(array $fids, &$context): void {
  if (!modularform_export_load_openspout()) {
    foreach ($fids as $fid) {
      $context['results']['errors'][] = t('OpenSpout library not available for form @fid.', ['@fid' => $fid]);
    }
    return;
  }

  // One call — 4 queries for all forms in chunk.
  $all_elements = _modularform_load_elements($fids);

  // Bulk-fetch all response meta for the entire chunk in one query.
  $response_rows = db_select('modularform_responses', 'r')
    ->fields('r', ['id', 'form_id', 'email'])
    ->condition('r.form_id', $fids, 'IN')
    ->orderBy('r.form_id')
    ->orderBy('r.id')
    ->execute()
    ->fetchAll();

  if (empty($response_rows)) {
    watchdog('modularform_export', 'No responses found for forms @fids, skipping.', [
      '@fids' => implode(', ', $fids),
    ], WATCHDOG_WARNING);
    return;
  }

  // Group response meta by form_id.
  $responses_by_form = [];
  foreach ($response_rows as $row) {
    $responses_by_form[(int) $row->form_id][$row->id] = $row;
  }

  // Bulk-fetch all answer data for every response in this chunk at once.
  $all_response_ids = array_column($response_rows, 'id');
  $all_answer_data  = _modularform_load_response_data($all_response_ids);

  // Write one CSV per form.
  foreach ($fids as $fid) {
    $elements = $all_elements[$fid] ?? [];

    if (empty($elements)) {
      watchdog('modularform_export', 'No elements found for form @fid, skipping.', ['@fid' => $fid], WATCHDOG_WARNING);
      continue;
    }

    $questions = _modularform_export_responses_extract_questions($elements);

    if (empty($questions)) {
      watchdog('modularform_export', 'No questions found for form @fid, skipping.', ['@fid' => $fid], WATCHDOG_WARNING);
      continue;
    }

    $form_responses = $responses_by_form[$fid] ?? [];

    if (empty($form_responses)) {
      watchdog('modularform_export', 'No responses found for form @fid, skipping.', ['@fid' => $fid], WATCHDOG_WARNING);
      continue;
    }

    $header = ['response_id', 'email'];
    foreach ($questions as $q_id => $question) {
      $header[] = $question['label'];
    }

    $real_filepath = drupal_realpath('private://modularform_exports') . '/responses_form_' . $fid . '.csv';

    try {
      $writer = new \OpenSpout\Writer\CSV\Writer();
      $writer->openToFile($real_filepath);
      $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($header));

      foreach ($form_responses as $response_id => $response_meta) {
        $email       = !empty($response_meta->email) ? $response_meta->email : t('anonymous');
        $answer_data = $all_answer_data[$response_id] ?? [];

        $row = [(int) $response_id, $email];
        foreach ($questions as $q_id => $question) {
          $row[] = _modularform_export_responses_format_answer(
            $answer_data[$q_id] ?? NULL
          );
        }

        $writer->addRow(\OpenSpout\Common\Entity\Row::fromValues($row));
      }

      $writer->close();
      $context['results']['files'][] = $real_filepath;
    }
    catch (Exception $e) {
      watchdog('modularform_export', 'Failed to write responses CSV for form @fid: @msg', [
        '@fid' => $fid,
        '@msg'  => $e->getMessage(),
      ], WATCHDOG_ERROR);
      $context['results']['errors'][] = t('Failed to export responses for form @fid.', ['@fid' => $fid]);
    }
  }

  $context['message'] = t('Exported responses for chunk of @count forms.', ['@count' => count($fids)]);
}

/**
 * Extracts ordered questions from an already-loaded elements array.
 *
 * Intended for bulk export paths where _modularform_load_elements() has
 * already been called for a chunk of forms. Avoids a redundant load by
 * accepting the elements array directly rather than a form ID.
 *
 * Sorting matches _modularform_export_responses_load_questions(): section
 * weight first, then question weight within each section.
 *
 * @param array $elements
 *   Elements array as returned by _modularform_load_elements() for a single
 *   form ID — keyed by element ID, each entry containing at minimum 'type',
 *   'parent', 'weight', 'label', and 'question_type'.
 *
 * @return array
 *   Associative array keyed by q_id, ordered by section then question weight:
 *   @code
 *   [
 *     $q_id => [
 *       'label'         => string,
 *       'question_type' => string,
 *     ],
 *     ...
 *   ]
 *   @endcode
 *   Returns an empty array if no questions are present.
 */
function _modularform_export_responses_extract_questions(array $elements): array {
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

  uasort($questions, function ($a, $b) use ($sections) {
    $sec_a = isset($sections[$a['parent']]) ? (int) $sections[$a['parent']]['weight'] : 0;
    $sec_b = isset($sections[$b['parent']]) ? (int) $sections[$b['parent']]['weight'] : 0;
    if ($sec_a !== $sec_b) {
      return $sec_a - $sec_b;
    }
    return (int) $a['weight'] - (int) $b['weight'];
  });

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
function _modularform_export_responses_format_answer($answer_entry): string {
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

  // File type — return comma-separated download URLs.
  if ($type === 'file') {
    $urls = array_column($answer_entry['files'] ?? [], 'url');
    return implode(', ', $urls);
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
