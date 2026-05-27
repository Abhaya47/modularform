<?php

/**
 * Returns raw, fully-hydrated answer data for a response.
 * used by load_defualt and solr document and api call
 *
 * @return array keyed by q_id:
 *   [
 *     'answer'   => $answer_row,
 *     'type'     => 'checkbox',   // resolved type name
 *     'multi'    => ['o_id1', 'o_id2'],        // raw ids/values
 *     'multi_labels' => ['Label A', 'Label B'], // resolved labels
 *   ]
 */
function _modularform_load_response_data($response_id) {
  $type_map     = array_flip(modularform_type_map());
  $datatype_map = modularform_datatype_map();

  $answers = db_select('modularform_answers', 'a')
    ->fields('a')
    ->condition('a.response_id', $response_id)
    ->execute()
    ->fetchAllAssoc('id');

  if (empty($answers)) {
    return [];
  }

  // Bulk fetch all multi rows
  $multi_rows = db_select('modularform_answers_multi', 'am')
    ->fields('am', ['answer_id', 'value'])
    ->condition('am.answer_id', array_keys($answers), 'IN')
    ->execute()
    ->fetchAll();

  // Bulk fetch all file rows
  $file_query = db_select('modularform_answer_files', 'af');
  $file_query->join('file_managed', 'fm', 'fm.fid = af.fid');
  $file_rows = $file_query
    ->fields('af', ['answer_id', 'fid'])
    ->fields('fm', ['filename', 'uri', 'filemime'])
    ->condition('af.answer_id', array_keys($answers), 'IN')
    ->execute()
    ->fetchAll();

  // Group file rows by answer_id
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

  // Separate tag ids from option ids (from multi rows)
  $tag_ids = [];
  $opt_ids = [];
  foreach ($multi_rows as $row) {
    $type_name = $type_map[(int) $answers[$row->answer_id]->datatype] ?? NULL;
    if ($type_name === 'tag') {
      $tag_ids[] = $row->value;
    }
    else {
      $opt_ids[] = $row->value;
    }
  }

  // Also collect radio option ids for label resolution
  foreach ($answers as $answer) {
    $type_name = $type_map[(int) $answer->datatype] ?? NULL;
    if ($type_name === 'radio' && !empty($answer->value_varchar)) {
      $opt_ids[] = $answer->value_varchar;
    }
  }

  // Bulk resolve
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
    $type_name = $type_map[(int) $answers[$row->answer_id]->datatype] ?? NULL;
    $multi_grouped[$row->answer_id]['ids'][]    = $row->value;
    $multi_grouped[$row->answer_id]['labels'][] = $type_name === 'tag'
      ? ($tags[$row->value]->title ?? $row->value)
      : ($options[$row->value]->option ?? $row->value);
  }

  // Build keyed by q_id
  $result = [];
  foreach ($answers as $answer) {
    $type_name = $type_map[(int) $answer->datatype] ?? 'text';
    $multi     = $multi_grouped[$answer->id] ?? [];

    // Decorate radio answers with resolved label
    if ($type_name === 'radio' && !empty($answer->value_varchar)) {
      $answer->option_label = $options[$answer->value_varchar]->option ?? NULL;
    }

    $result[$answer->q_id] = [
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

  return $result;
}
