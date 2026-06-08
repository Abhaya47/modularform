<?php

/**
 * Returns raw, fully-hydrated answer data for one or more responses.
 *
 * Used by load_default, Solr document builder, and API calls.
 *
 * @param int|int[] $response_id
 *   A single response ID or an array of response IDs.
 *
 * @return array
 *   When called with a single int, returns an array keyed by q_id:
 *   @code
 *   [
 *     $q_id => [
 *       'answer' => object,   // raw DB row from modularform_answers
 *       'type'   => string,   // resolved type name e.g. 'checkbox', 'radio'
 *       'multi'  => [         // empty array for non-multi types
 *         ['id' => mixed, 'label' => string],
 *         ...
 *       ],
 *       'files'  => [         // empty array for non-file types
 *         ['fid' => int, 'filename' => string, 'uri' => string,
 *          'url' => string, 'filemime' => string],
 *         ...
 *       ],
 *     ],
 *     ...
 *   ]
 *   @endcode
 *
 *   When called with an array of IDs, returns an array keyed by response_id,
 *   each value having the same q_id-keyed shape described above.
 *   Response IDs with no answers are present as empty arrays.
 */
function _modularform_load_response_data($response_id) {
  $scalar       = !is_array($response_id);
  $response_ids = array_values(array_unique(array_map('intval', (array) $response_id)));

  if (empty($response_ids)) {
    return $scalar ? [] : [];
  }

  $type_map     = array_flip(modularform_type_map());
  $datatype_map = modularform_datatype_map();

  // --- Answers ---
  $answers_raw = db_select('modularform_answers', 'a')
    ->fields('a')
    ->condition('a.response_id', $response_ids, 'IN')
    ->execute()
    ->fetchAllAssoc('id');

  if (empty($answers_raw)) {
    if ($scalar) {
      return [];
    }
    return array_fill_keys($response_ids, []);
  }

  $answer_ids = array_keys($answers_raw);

  // --- Multi rows ---
  $multi_rows = db_select('modularform_answers_multi', 'am')
    ->fields('am', ['answer_id', 'value'])
    ->condition('am.answer_id', $answer_ids, 'IN')
    ->execute()
    ->fetchAll();

  // --- File rows ---
  $file_query = db_select('modularform_answer_files', 'af');
  $file_query->join('file_managed', 'fm', 'fm.fid = af.fid');
  $file_rows = $file_query
    ->fields('af', ['answer_id', 'fid'])
    ->fields('fm', ['filename', 'uri', 'filemime'])
    ->condition('af.answer_id', $answer_ids, 'IN')
    ->execute()
    ->fetchAll();

  // Group files by answer_id
  $files_grouped = [];
  foreach ($file_rows as $row) {
    $files_grouped[$row->answer_id][] = [
      'fid'      => $row->fid,
      'filename' => $row->filename,
      'uri'      => $row->uri,
      'url'      => file_create_url($row->uri),
      'filemime' => $row->filemime,
    ];
  }

  // Collect tag/option IDs to resolve
  $tag_ids = [];
  $opt_ids = [];

  foreach ($multi_rows as $row) {
    $type_name = $type_map[(int) $answers_raw[$row->answer_id]->datatype] ?? NULL;
    if ($type_name === 'tag') {
      $tag_ids[] = $row->value;
    }
    else {
      $opt_ids[] = $row->value;
    }
  }
  foreach ($answers_raw as $answer) {
    $type_name = $type_map[(int) $answer->datatype] ?? NULL;
    if ($type_name === 'radio' && !empty($answer->value_varchar)) {
      $opt_ids[] = $answer->value_varchar;
    }
  }

  // Bulk resolve labels
  $tags = !empty($tag_ids)
    ? db_select('modularform_tags', 't')
      ->fields('t', ['id', 'title'])
      ->condition('t.id', array_unique($tag_ids), 'IN')
      ->execute()->fetchAllAssoc('id')
    : [];

  $options = !empty($opt_ids)
    ? db_select('modularform_question_options', 'o')
      ->fields('o', ['o_id', 'option'])
      ->condition('o.o_id', array_unique($opt_ids), 'IN')
      ->execute()->fetchAllAssoc('o_id')
    : [];

  // Group multi rows by answer_id
  $multi_grouped = [];
  foreach ($multi_rows as $row) {
    $type_name = $type_map[(int) $answers_raw[$row->answer_id]->datatype] ?? NULL;
    $multi_grouped[$row->answer_id]['ids'][]    = $row->value;
    $multi_grouped[$row->answer_id]['labels'][] = $type_name === 'tag'
      ? ($tags[$row->value]->title ?? $row->value)
      : ($options[$row->value]->option ?? $row->value);
  }

  // Build result keyed by response_id => q_id
  $results = array_fill_keys($response_ids, []);

  foreach ($answers_raw as $answer) {
    $type_name = $type_map[(int) $answer->datatype] ?? 'text';
    $multi     = $multi_grouped[$answer->id] ?? [];

    if ($type_name === 'radio' && !empty($answer->value_varchar)) {
      $answer->option_label = $options[$answer->value_varchar]->option ?? NULL;
    }

    $results[(int) $answer->response_id][$answer->q_id] = [
      'answer' => $answer,
      'type'   => $type_name,
      'multi'  => array_map(
        fn($id, $label) => ['id' => $id, 'label' => $label],
        $multi['ids']    ?? [],
        $multi['labels'] ?? []
      ),
      'files'  => $files_grouped[$answer->id] ?? [],
    ];
  }

  return $scalar ? ($results[$response_ids[0]] ?? []) : $results;
}
