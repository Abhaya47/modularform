<?php

module_load_include('inc', 'modularform', 'modularform.rules');
module_load_include('inc', 'modularform', 'includes/load_response_data');

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

  $document->id              = 'response_' . $response_id;
  $document->ss_doc_type     = 'response';
  $document->is_form_id      = (int) $response->form_id;
  $document->ss_form_label   = $form->title;
  $document->is_user_id      = (int) $response->user_id;
  $document->ss_email        = $response->email;
  $document->is_submitted_at = (int) $response->submitted_at;

  $data = _modularform_load_response_data($response_id);

  foreach ($data as $q_id => $entry) {
    $answer = $entry['answer'];

    // For radio/dropdown, swap stored o_id for human label before passing to mapper
    if (in_array($entry['type'], ['radio', 'dropdown']) && !empty($answer->option_label)) {
      $answer->value_varchar = $answer->option_label;
    }

    $mapped = modularform_search_answer_to_solr_field($answer, $entry['multi_labels']);
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
