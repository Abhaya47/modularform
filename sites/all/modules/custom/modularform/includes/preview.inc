<?php


/**
 * Preview function
 */
function modularform_preview_form($form, &$form_state, $fid) {

  $elements = _modularform_load_elements($fid);

  _modularform_build_fields($form, $elements, 'preview');

  return $form;
}
