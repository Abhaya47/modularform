<?php

module_load_include('inc', 'modularform', 'modularform.rules');

function modularform_search_response_document_callback($document, $entity, $entity_type, $env_id) {

  $response_id = is_object($entity) ? $entity->id : $entity;

  $response = db_select('modularform_responses', 'r')
    ->fields('r')
    ->condition('r.id', $response_id)
    ->execute()
    ->fetchObject();
  if (!$response) {
    return $document;
  }

  $form = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title'])
    ->condition('f.id', $response->form_id)
    ->execute()
    ->fetchObject();
  if (!$form) {
    return [];
  }

  $answers = db_select('modularform_answers', 'a')
    ->fields('a')
    ->condition('a.response_id', $response_id)
    ->execute()
    ->fetchAllAssoc('id');

  $type_map     = array_flip(modularform_type_map());
  $solr_field_map = solr_field_map();

  // Classify answers using solr_field_map
  $answer_ids        = !empty($answers) ? array_keys($answers) : [];
  $tag_answer_ids    = [];
  $opt_answer_ids    = [];  // radio, dropdown, checkbox, list, dropdown
  foreach ($answers as $answer) {
    $type_int  = (int) $answer->datatype;
    $type_name = $type_map[$type_int] ?? NULL;
    $solr_field = $solr_field_map[$type_name] ?? 'ss_val';
    if ($solr_field === 'sm_val') {
      if ($type_name === 'tag') {
        $tag_answer_ids[] = $answer->id;
      } else {
        $opt_answer_ids[] = $answer->id;
      }
    }
  }


  // Fetch all multi rows in one query
  $multi_answers = [];
  if (!empty($answer_ids)) {
    $multi_answers = db_select('modularform_answers_multi', 'am')
      ->fields('am', ['answer_id', 'value'])
      ->condition('am.answer_id', $answer_ids, 'IN')
      ->execute()
      ->fetchAll();
  }

  // Collect IDs to resolve
  $tag_value_ids = [];
  $opt_value_ids = [];
  foreach ($multi_answers as $row) {
    $type_int  = (int) $answers[$row->answer_id]->datatype;
    $type_name = $type_map[$type_int] ?? NULL;
    if ($type_name === 'tag') {
      $tag_value_ids[] = $row->value;
    } else {
      $opt_value_ids[] = $row->value;
    }
  }

  // Lookup tag titles
  $tags = [];
  if (!empty($tag_value_ids)) {
    $tags = db_select('modularform_tags', 't')
      ->fields('t', ['id', 'title'])
      ->condition('t.id', $tag_value_ids, 'IN')
      ->execute()
      ->fetchAllAssoc('id');
  }

  // Lookup multi option labels (checkbox, list)
  $options = [];
  if (!empty($opt_value_ids)) {
    $options = db_select('modularform_question_options', 'o')
      ->fields('o', ['o_id', 'option'])
      ->condition('o.o_id', $opt_value_ids, 'IN')
      ->execute()
      ->fetchAllAssoc('o_id');
  }

  // Group resolved multi labels by answer ID
  $multi_grouped = [];
  foreach ($multi_answers as $row) {
    $type_int  = (int) $answers[$row->answer_id]->datatype;
    $type_name = $type_map[$type_int] ?? NULL;
    if ($type_name === 'tag') {
      $multi_grouped[$row->answer_id][] = isset($tags[$row->value])
        ? $tags[$row->value]->title
        : $row->value;
    } else {
      $multi_grouped[$row->answer_id][] = isset($options[$row->value])
        ? $options[$row->value]->option
        : $row->value;
    }
  }

  // Lookup single-select option labels (radio, dropdown)
  $single_option_ids = [];
  foreach ($answers as $answer) {
    $type_name = $type_map[(int) $answer->datatype] ?? NULL;
    $solr_field = $solr_field_map[$type_name] ?? 'ss_val';
    if ($solr_field !== 'sm_val' && in_array($type_name, ['radio', 'dropdown']) && !empty($answer->value_varchar)) {
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

  // Build Solr document
  $document->id              = 'response_' . $response_id;
  $document->ss_doc_type     = 'response';
  $document->is_form_id      = (int) $response->form_id;
  $document->ss_form_label   = $form->title;
  $document->is_user_id      = (int) $response->user_id;
  $document->ss_email        = $response->email;
  $document->is_submitted_at = (int) $response->submitted_at;

  foreach ($answers as $answer) {
    $q_id      = $answer->q_id;
    $type_int  = (int) $answer->datatype;
    $type_name = $type_map[$type_int] ?? NULL;
    $solr_field = $solr_field_map[$type_name] ?? 'ss_val';

    // Resolve single-select stored value → human label
    if (in_array($type_name, ['radio', 'dropdown']) && !empty($answer->value_varchar)) {
      $o_id = $answer->value_varchar;
      $answer->value_varchar = isset($single_options[$o_id])
        ? $single_options[$o_id]->option
        : $o_id;
    }

    $multi_vals = ($solr_field === 'sm_val' && isset($multi_grouped[$answer->id]))
      ? $multi_grouped[$answer->id]
      : [];

    $mapped = modularform_search_answer_to_solr_field($answer, $multi_vals);
    if ($mapped !== NULL) {
      $field            = $mapped['field'] . '_' . $q_id;
      $document->$field = $mapped['value'];
    }
  }

  return [$document];
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
  $type_int = (int) $answer->datatype;
  $type_name = isset($type_map[$type_int]) ? $type_map[$type_int] : NULL;
  $storage = isset($datatype_map[$type_name]) ? $datatype_map[$type_name] : 'varchar';
  $solr_field = isset($solr_field_map[$type_name]) ? $solr_field_map[$type_name] : 'ss_val';
  // Multi
  if ($solr_field === 'sm_val') {
    return [
      'field' => 'sm_val',
      'value' => array_map('strval', $multi_values),
    ];
  }
  // Single
  $value_col = isset($value_col_map[$storage]) ? $value_col_map[$storage] : 'value_varchar';
  $raw = $answer->{$value_col};
  if ($raw === NULL) {
    return NULL;
  }
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
  db_update('apachesolr_index_entities')
    ->condition('entity_type', 'modularform_response')
    ->fields(['changed' => REQUEST_TIME, 'status' => 1])
    ->execute();
}
