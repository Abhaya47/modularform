<?php
function modularform_search_bar_form($form, &$form_state, $current_search = '') {
  $form['#method'] = 'get';
  $form['#action'] = url('forms/manage');
  $form['#theme_wrappers'] = ['form'];
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
    '#type'       => 'submit',
    '#value'      => t('Search'),
    '#attributes' => ['class' => ['modularform-search-btn']],
  ];

  if (!empty($current_search)) {
    $form['clear'] = [
      '#type'   => 'markup',
      '#markup' => l(t('✕ Clear'), 'forms/manage', [
        'attributes' => ['class' => ['modularform-search-clear']],
      ]),
    ];
  }

  // Leave form_build_id, form_token, form_id alone — hidden inputs don't affect layout
  return $form;
}

function modularform_search_bar_form_submit($form, &$form_state) {}
