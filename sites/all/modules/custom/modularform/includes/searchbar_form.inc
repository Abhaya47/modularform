<?php


function modularform_search_bar_form($form, &$form_state, $current_search = '') {
  $form['#method'] = 'get';  // Use GET so URL stays shareable/bookmarkable
  $form['#action'] = url(current_path());

  $form['search'] = [
    '#type'          => 'textfield',
    '#title'         => t('Search Forms'),
    '#title_display' => 'invisible',
    '#default_value' => $current_search,
    '#placeholder'   => t('Search by form name...'),
    '#size'          => 40,
    '#attributes'    => ['class' => ['modularform-search-input']],
  ];

  $form['submit'] = [
    '#type'  => 'submit',
    '#value' => t('Search'),
    '#attributes' => ['class' => ['modularform-search-btn']],
  ];

  // Show clear button only when search is active
  if (!empty($current_search)) {
    $form['clear'] = [
      '#type'   => 'markup',
      '#markup' => l(t('Clear'), current_path(), [
        'attributes' => ['class' => ['modularform-search-clear']],
      ]),
    ];
  }

  // Prevent Drupal from appending form_build_id etc to GET params
  $form['form_build_id']['#access'] = FALSE;
  $form['form_token']['#access']    = FALSE;
  $form['form_id']['#access']       = FALSE;

  return $form;
}

/**
 * Submit handler — not really needed for GET forms but required by Drupal.
 */
function modularform_search_bar_form_submit($form, &$form_state) {
  // GET form handles itself via URL
}

function modularform_search_ajax() {
  $term = isset($_GET['term']) ? check_plain(trim($_GET['term'])) : '';
  $html = modularform_build_forms_table($term);

  $commands   = [];
  $commands[] = ajax_command_html('#modularform-dashboard-table-wrapper', $html);

  return ['#type' => 'ajax', '#commands' => $commands];
}
