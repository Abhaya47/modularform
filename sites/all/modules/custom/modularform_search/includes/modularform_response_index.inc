<?php

module_load_include('inc', 'modularform', 'modularform.rules');

function modularform_search_response_document_callback($document, $entity, $entity_type, $env_id) {
  $response_id = is_object($entity) ? $entity->id : $entity;

  // Load all answers for this response
  $answers = db_select('modularform_answers', 'a')
    ->fields('a')
    ->condition('a.response_id', $response_id)
    ->execute()
    ->fetchAllAssoc('id');

  // Load response itself for parent doc fields
  $response = db_select('modularform_responses', 'r')
    ->fields('r')
    ->condition('r.id', $response_id)
    ->execute()
    ->fetchObject();


  if (!$response) {
    return $document;
  }

  // Load form for label
  $form = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title'])
    ->condition('f.id', $response->form_id)
    ->execute()
    ->fetchObject();

  if (!$form) {
    return [];
  }

  // Load all questions in one query
  $q_ids = !empty($answers) ? array_map(function($a) {
    return $a->q_id;
  }, $answers) : [];
  $questions = [];
  if (!empty($q_ids)) {
    $questions = db_select('modularform_questions', 'q')
      ->fields('q', ['q_id', 'label', 'type'])
      ->condition('q.q_id', $q_ids, 'IN')
      ->execute()
      ->fetchAllAssoc('q_id');
  }

  // Load multi answers in one query
  $answer_ids = !empty($answers) ? array_keys($answers) : [];
  $multi_answers = [];
  if (!empty($answer_ids)) {
    $multi_answers = db_select('modularform_answers_multi', 'am')
      ->fields('am', ['answer_id', 'value'])
      ->condition('am.answer_id', $answer_ids, 'IN')
      ->execute()
      ->fetchAll();
  }

  // Resolve option texts in one query
  $option_ids = array_map(function($m) {
    return $m->value;
  }, $multi_answers);
  $options = [];
  if (!empty($option_ids)) {
    $options = db_select('modularform_question_options', 'o')
      ->fields('o', ['o_id', 'option'])
      ->condition('o.o_id', $option_ids, 'IN')
      ->execute()
      ->fetchAllAssoc('o_id');
  }

  // Group multi values by answer_id
  // Group multi values by answer_id
  $multi_grouped = [];
  foreach ($multi_answers as $row) {
    $multi_grouped[$row->answer_id][] = isset($options[$row->value])
      ? $options[$row->value]->option
      : $row->value;
  }

  // Resolve single-select option labels
  $single_option_ids = [];
  foreach ($answers as $answer) {
    $type_int = (int) $answer->datatype;
    if (in_array($type_int, [2, 6]) && !empty($answer->value_varchar)) {
      $single_option_ids[] = $answer->value_varchar;
    }
  }

  $single_options = [];
  if (!empty($single_option_ids)) {
    $single_options = db_select('modularform_question_options', 'o')
      ->fields('o', ['o_id', 'option'])
      ->condition('o.o_id', $single_option_ids, 'IN')
      ->execute()
      ->fetchAllAssoc('o_id');
  }

  // Build parent doc
  $solr_doc = [
    'id' => 'response_' . $response_id,
    'ss_doc_type' => 'response',
    'is_form_id' => (int) $response->form_id,
    'ss_form_label'   => $form->title,        // add this
    'is_user_id' => (int) $response->user_id,
    'ss_email' => $response->email,
    'is_submitted_at' => (int) $response->submitted_at,
    '_childDocuments_' => [],
  ];

  // Build child docs
  // Derived directly from modularform_type_map() + modularform_datatype_map()
  $single_option_types = [2,6];    // radio=2, dropdown=6 — store opt_* in value_varchar
  $multi_types = [4,7,11]; // checkbox=4, tag=7, list=11 — store in answers_multi

  foreach ($answers as $answer) {
    $q_id = $answer->q_id;
    $type_int = (int) $answer->datatype;
    $q_label = isset($questions[$q_id]) ? $questions[$q_id]->label : $q_id;

    $child = [
      'id' => 'answer_' . $answer->id,
      'ss_doc_type' => 'answer',
      'is_form_id'  => (int) $response->form_id,
      'ss_form_label'   => $form->title,
      'ss_q_id' => $q_id,
      'ss_q_label' => $q_label,
      'is_q_type' => $type_int,
    ];

    // Resolve opt_* → label BEFORE field mapper reads value_varchar
    if (in_array($type_int, $single_option_types) && !empty($answer->value_varchar)) {
      $o_id = $answer->value_varchar;
      $answer->value_varchar = isset($single_options[$o_id])
        ? $single_options[$o_id]->option
        : $o_id; // fallback to raw id so at least something is indexed
    }

    // Gate multi values strictly — only checkbox/tag/list get them
    $multi_vals = (in_array($type_int, $multi_types) && isset($multi_grouped[$answer->id]))
      ? $multi_grouped[$answer->id]
      : [];

    $mapped = modularform_search_answer_to_solr_field($answer, $multi_vals);

    if ($mapped !== NULL) {
      $child[$mapped['field']] = $mapped['value'];
    }

    $solr_doc['_childDocuments_'][] = $child;
  }

  // Send directly to Solr 9
  modularform_search_send_to_solr($solr_doc);

  // Return empty document so Apache Solr module doesn't try to index anything
  return [];
}

/**
 * Sends a nested document to Solr 9 via JSON API.
 */
function modularform_search_send_to_solr(array $doc) {
  $solr = apachesolr_get_solr();
  $url = $solr->getUrl() . 'update?commit=true';

  $response = drupal_http_request($url, [
    'method' => 'POST',
    'data' => json_encode([$doc]),
    'headers' => [
      'Content-Type' => 'application/json',
    ],
  ]);

  if ($response->code != 200) {
    watchdog('modularform_search', 'Solr indexing failed for doc @id: @error', [
      '@id' => $doc['id'],
      '@error' => $response->data,
    ], WATCHDOG_ERROR);
  }
}

/**
 * Maps an answer row to the correct Solr field name and typed value.
 *
 * @param object $answer Row from modularform_answers
 * @param array $multi_values Resolved option texts for this answer (if multi)
 *
 * @return array ['field' => string, 'value' => mixed]
 */
function modularform_search_answer_to_solr_field($answer, array $multi_values = []) {
  $type_map = array_flip(modularform_type_map());
  $datatype_map = modularform_datatype_map();
  $solr_field_map = solr_field_map();
  $value_col_map = value_col_map();


  $multi_types = [4, 7, 11]; // checkbox, tag, list
  $type_int = (int) $answer->datatype;
  $type_name = isset($type_map[$type_int]) ? $type_map[$type_int] : NULL;
  $storage = isset($datatype_map[$type_name]) ? $datatype_map[$type_name] : 'varchar';

  // Multi
  if (in_array($type_int, $multi_types)) {
    return [
      'field' => 'sm_val',  // sm_* is multiValued string
      'value' => $multi_values,
    ];
  }

  // Single
  $value_col = isset($value_col_map[$storage]) ? $value_col_map[$storage] : 'value_varchar';
  $raw = $answer->{$value_col};

  if ($raw === NULL) {
    return NULL; // nothing to index
  }

  $solr_field = isset($solr_field_map[$storage]) ? $solr_field_map[$storage] : 'val_ss';

  $typed_value = $raw;
  if ($storage === 'date') {
    $typed_value = gmdate('Y-m-d\TH:i:s\Z', $raw);
  }
  elseif ($storage === 'bool') {
    $typed_value = (bool) $raw;
  }
  elseif ($storage === 'int') {
    $typed_value = (int) $raw;
  }
  else {
    $typed_value = (string) $raw;
  }

  return [
    'field' => $solr_field,
    'value' => $typed_value,
  ];
}

function modularform_search_response_status_callback($entity, $entity_type) {
  // Entity may come in as id directly
  $entity_id = is_object($entity) ? $entity->id : $entity;

  $query = db_select('modularform_responses', 'r');
  $query->join('modularform_forms', 'f', 'f.id = r.form_id');
  $query->fields('r', ['id'])
    ->fields('f', ['deleted_at'])
    ->condition('r.id', $entity_id);

  $result = $query->execute()->fetchObject();

  return $result && empty($result->deleted_at);
}

function modularform_search_response_reindex_callback($env_id) {
  // Wipe existing queue
  db_truncate('modularform_response_solr_queue')->execute();

  // Re-queue all responses
  $responses = db_select('modularform_responses', 'r')
    ->fields('r', ['id'])
    ->execute();

  foreach ($responses as $response) {
    db_merge('modularform_response_solr_queue')
      ->key(['entity_id' => $response->id])
      ->fields([
        'entity_type' => 'modularform_response',
        'entity_id' => $response->id,
        'bundle' => 'modularform_response',
        'status' => 1,
        'changed' => REQUEST_TIME,
      ])
      ->execute();
  }
}
