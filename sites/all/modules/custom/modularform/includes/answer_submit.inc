<?php

require_once dirname(__FILE__) . '/builder_build_fields.inc';

function modularform_answer_form($form, &$form_state, $fid) {
  $elements = _modularform_load_elements($fid);
  _modularform_build_fields($form, $elements, 'submit');

  $form['submit'] = [
    '#type' => 'submit',
    '#value' => t('Submit'),
  ];
  return $form;
}

function modularform_answer_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  var_dump($values);
  die();
  // iterate sections/questions, save answers to DB
}
