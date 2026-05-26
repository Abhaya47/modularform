<?php

module_load_include('inc', 'modularform', 'answer_form');

function modularform_response_view_answer($form, &$form_state, $response_id) {
  $response = db_select('modularform_responses', 'r')
    ->fields('r')
    ->condition('r.id', $response_id)
    ->execute()
    ->fetchObject();

  $elements = _modularform_load_elements($response->form_id);
  $defaults = _modularform_load_defaults($response_id);
  _modularform_build_fields($form, $elements, $response->form_id, 'view', $defaults);
  return $form;
}
