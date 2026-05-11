<?php
module_load_include('inc', 'modularform', 'modularform.rules');

#TODO sepeate sections into pages
function modularform_builder_form($form, &$form_state, $fid) {
  $form['#tree'] = TRUE;
  $form_state['storage']['fid'] = $fid;

  $form_name = db_select('modularform_forms', 'f')
    ->fields('f', ['title', 'description'])
    ->condition('id', $fid)
    ->execute()
    ->fetchAssoc();

  $form['form_header'] = [
    '#type'   => 'item',
    '#markup' => '<div class="modularform-header-box">
      <div class="modularform-header-bar"></div>
      <div style="padding: 28px 32px 32px;">
        <h2 class="modularform-title">' . check_plain($form_name['title']) . '</h2>
        <div class="modularform-description">' . nl2br(check_plain($form_name['description'])) . '</div>
      </div>
    </div>',
    '#weight' => -10,
  ];

  // ── Seed storage ────────────────────────────────────────────────────────────
  if (empty($form_state['storage']['elements'])) {
    $loaded = _modularform_load_elements($fid);
    if ($loaded) {
      $form_state['storage']['is_edit']    = TRUE;
      $form_state['storage']['elements']   = $loaded;
    }
    else {
      $first_sec_id = 'sec_' . $fid . '_' . uniqid();
      $form_state['storage']['is_edit']    = FALSE;
      $form_state['storage']['elements'][$first_sec_id] = [
        'type'   => 'section',
        'title'  => 'Section 1',
        'weight' => 0,
        'parent' => NULL,
      ];
    }
  }

  $is_edit = $form_state['storage']['is_edit'] ?? FALSE;

  // ── Sync submitted values back into storage ─────────────────────────────────
  if (!empty($form_state['values'])) {
    _modularform_sync_storage($form_state);
  }

  $elements = $form_state['storage']['elements'];

  // ── Bucket + sort ───────────────────────────────────────────────────────────
  $sections  = array_filter($elements, fn($el) => $el['type'] === 'section');
  $questions = array_filter($elements, fn($el) => $el['type'] === 'question');
  $options   = array_filter($elements, fn($el) => $el['type'] === 'option');
  $val_rules = array_filter($elements, fn($el) => $el['type'] === 'validation_rule');

  uasort($sections,  fn($a, $b) => ($a['weight'] ?? 0) <=> ($b['weight'] ?? 0));
  uasort($questions, fn($a, $b) => ($a['weight'] ?? 0) <=> ($b['weight'] ?? 0));
  uasort($options,   fn($a, $b) => ($a['weight'] ?? 0) <=> ($b['weight'] ?? 0));

  $section_keys = array_keys($sections);

  $form['elements_container'] = [
    '#type'       => 'container',
    '#attributes' => ['id' => ['modular-form-wrapper']],
  ];

  // ── Render sections ─────────────────────────────────────────────────────────
  foreach ($sections as $sec_id => $sec_el) {
    $form['elements_container'][$sec_id] = [
      '#type'       => 'container',
      '#attributes' => [
        'id'    => 'section-wrapper-' . $sec_id,
        'class' => ['depth-section'],
      ],
    ];

    $form['elements_container'][$sec_id]['title'] = [
      '#type'          => 'textfield',
      '#title'         => t('Section Title'),
      '#prefix'        => '<div class="drag-handle" data-drag-scope="section" title="Drag to reorder">
                             <span></span><span></span><span></span>
                           </div>',
      '#default_value' => $sec_el['title'] ?? '',
    ];

    $form['elements_container'][$sec_id]['weight'] = [
      '#type'          => 'hidden',
      '#default_value' => $sec_el['weight'] ?? 0,
      '#attributes'    => ['class' => ['section-weight']],
    ];

    $section_questions = array_filter($questions, fn($el) => $el['parent'] === $sec_id);

    foreach ($section_questions as $q_id => $q_el) {
      _modularform_render_question($form, $form_state, $sec_id, $q_id, $q_el, $options, $val_rules);
    }

    $form['elements_container'][$sec_id]['add_q'] = [
      '#type'   => 'submit',
      '#value'  => t('+ Add Question'),
      '#name'   => 'add_q_' . $sec_id,
      '#submit' => ['modularform_add_element_submit'],
      '#ajax'   => [
        'callback' => 'modularform_ajax_section_callback',
        'wrapper'  => 'section-wrapper-' . $sec_id,
      ],
    ];

    if ($sec_id !== $section_keys[0]) {
      $form['elements_container'][$sec_id]['delete'] = [
        '#type'                    => 'submit',
        '#value'                   => t('Delete Section'),
        '#name'                    => 'delete_' . $sec_id,
        '#submit'                  => ['modularform_delete_element_submit'],
        '#ajax'                    => [
          'callback' => 'modularform_ajax_top_callback',
          'wrapper'  => 'modular-form-wrapper',
        ],
        '#limit_validation_errors' => [],
      ];
    }
  }

  $form['elements_container']['add_section'] = [
    '#type'   => 'submit',
    '#value'  => t('+ Add Section'),
    '#name'   => 'add_section',
    '#submit' => ['modularform_add_element_submit'],
    '#ajax'   => [
      'callback' => 'modularform_ajax_top_callback',
      'wrapper'  => 'modular-form-wrapper',
    ],
  ];

  $form['elements_container']['add_element'] = [
    '#type'  => 'submit',
    '#value' => $is_edit ? t('Update survey') : t('Create survey'),
  ];

  $form['#attached']['js'][] = drupal_get_path('module', 'modularform') . '/js/modularform_dnd.js';

  return $form;
}

// ── Sync ─────────────────────────────────────────────────────────────────────

function _modularform_sync_storage(&$form_state) {
  $elements = &$form_state['storage']['elements'];
  $values   = $form_state['values']['elements_container'];

  foreach ($elements as $id => &$el) {
    switch ($el['type']) {
      case 'section':
        $w = drupal_array_get_nested_value($values, [$id, 'weight']);
        if (isset($w)) {
          $el['weight'] = (int) $w;
        }
        break;

      case 'question':
        _modularform_sync_question($form_state, $id, $el, $elements, $values);
        break;

      case 'option':
        $q_parent  = $el['parent'];
        $sec_parent = $elements[$q_parent]['parent'] ?? NULL;
        if ($sec_parent) {
          $w = drupal_array_get_nested_value($values, [$sec_parent, $q_parent, $id, 'weight']);
          if (isset($w)) {
            $el['weight'] = (int) $w;
          }
        }
        break;
    }
  }
  unset($el);
}

function _modularform_sync_question(&$form_state, $id, &$el, &$elements, $values) {
  $sec_id = $el['parent'];

  // Weight
  $w = drupal_array_get_nested_value($values, [$sec_id, $id, 'weight']);
  if (isset($w)) {
    $el['weight'] = (int) $w;
  }

  // Parent section reassignment
  $new_sec = drupal_array_get_nested_value($values, [$sec_id, $id, 'parent_sec']);
  if (isset($new_sec) && $new_sec !== $el['parent']) {
    $el['parent'] = $new_sec;
  }

  // Question type
  $submitted_type = drupal_array_get_nested_value($values, [$sec_id, $id, 'question_type']);
  if (isset($submitted_type)) {
    $old_type    = $el['question_type'] ?? 'text';
    $had_options = in_array($old_type,        ['radio', 'checkbox', 'dropdown', 'list', 'tag']);
    $has_options = in_array($submitted_type,  ['radio', 'checkbox', 'dropdown', 'list', 'tag']);

    $el['question_type'] = $submitted_type;

    if ($had_options && !$has_options) {
      foreach ($elements as $child_id => $child_el) {
        if ($child_el['type'] === 'option' && $child_el['parent'] === $id) {
          unset($elements[$child_id]);
        }
      }
    }
  }

  // Add validation toggle
  $submitted_adv = drupal_array_get_nested_value($values, [$sec_id, $id, 'validation', 'add_validation']);
  if (isset($submitted_adv)) {
    $was_enabled = $el['add_validation'] ?? FALSE;
    $now_enabled = (bool) $submitted_adv;
    $el['add_validation'] = $now_enabled;

    if ($was_enabled && !$now_enabled) {
      foreach ($elements as $child_id => $child_el) {
        if ($child_el['type'] === 'validation_rule' && $child_el['parent'] === $id) {
          unset($elements[$child_id]);
        }
      }
    }
  }

  // Rule types
  $q_val_rules = array_filter($elements, fn($el) => $el['type'] === 'validation_rule' && $el['parent'] === $id);
  foreach ($q_val_rules as $r_id => $r_el) {
    $submitted_rule_type = drupal_array_get_nested_value($values, [
      $sec_id, $id, 'validation', 'adv_rules', $r_id, 'rule_type',
    ]);
    if (isset($submitted_rule_type)) {
      $elements[$r_id]['rule_type'] = $submitted_rule_type;
    }
  }
}

// ── Render helpers ────────────────────────────────────────────────────────────

function _modularform_render_question(&$form, &$form_state, $sec_id, $q_id, $q_el, $options, $val_rules) {
  $q_type       = $q_el['question_type'] ?? 'text';
  $show_adv_val = $q_el['add_validation'] ?? FALSE;

  $form['elements_container'][$sec_id][$q_id] = [
    '#type'       => 'container',
    '#attributes' => [
      'id'    => 'question-wrapper-' . $q_id,
      'class' => ['depth-question'],
    ],
  ];

  $form['elements_container'][$sec_id][$q_id]['question_text'] = [
    '#type'          => 'textfield',
    '#title'         => t('Question'),
    '#default_value' => $q_el['label'] ?? '',
  ];

  $form['elements_container'][$sec_id][$q_id]['drag_handle'] = [
    '#markup' => '<div class="drag-handle" data-drag-scope="question" title="Drag to reorder"><span></span><span></span><span></span></div>',
    '#weight' => -99,
  ];

  $form['elements_container'][$sec_id][$q_id]['weight'] = [
    '#type'          => 'hidden',
    '#default_value' => $q_el['weight'] ?? 0,
    '#attributes'    => ['class' => ['question-weight']],
  ];

  $form['elements_container'][$sec_id][$q_id]['parent_sec'] = [
    '#type'          => 'hidden',
    '#default_value' => $sec_id,
    '#attributes'    => ['class' => ['question-parent-sec']],
  ];

  $form['elements_container'][$sec_id][$q_id]['question_type'] = [
    '#type'          => 'select',
    '#title'         => t('Question Type'),
    '#options'       => [
      'text'      => t('Short Answer'),
      'textarea'  => t('Paragraph'),
      'richtext'  => t('Rich Text'),
      'date'      => t('Date'),
      'time'      => t('Time'),
      'list'      => t('List'),
      'radio'     => t('Multiple Choice'),
      'boolean'   => t('Yes / No'),
      'checkbox'  => t('Checkbox'),
      'dropdown'  => t('Dropdown'),
      'file'      => t('File'),
      'tag'       => t('Tag'),
    ],
    '#default_value' => $q_type,
    '#ajax'          => [
      'callback' => 'modularform_ajax_question_callback',
      'wrapper'  => 'question-wrapper-' . $q_id,
      'event'    => 'change',
    ],
  ];

  _modularform_render_question_type($form, $form_state, $sec_id, $q_id, $q_type, $options, $val_rules);

  _modularform_render_question_validation($form, $form_state, $sec_id, $q_id, $q_type, $q_el, $val_rules, $show_adv_val);

  $form['elements_container'][$sec_id][$q_id]['delete'] = [
    '#type'                    => 'submit',
    '#value'                   => t('Delete Question'),
    '#name'                    => 'delete_' . $q_id,
    '#submit'                  => ['modularform_delete_element_submit'],
    '#ajax'                    => [
      'callback' => 'modularform_ajax_section_callback',
      'wrapper'  => 'section-wrapper-' . $sec_id,
    ],
    '#limit_validation_errors' => [],
  ];
}

function _modularform_render_question_type(&$form, &$form_state, $sec_id, $q_id, $q_type, $options, $val_rules) {
  $is_multiple = _modularform_get_is_multiple($q_id, $val_rules);

  switch ($q_type) {
    case 'text':
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">Short answer text field</p>',
      ];
      break;

    case 'textarea':
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">Long answer text area</p>',
      ];
      break;

    case 'richtext':
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">Rich text editor (TinyMCE)</p>',
      ];
      break;

    case 'date':
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">Date picker</p>',
      ];
      break;

    case 'time':
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">Time picker</p>',
      ];
      break;

    case 'boolean':
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">Yes / No</p>',
      ];
      break;

    case 'file':
      $form['elements_container'][$sec_id][$q_id]['file_settings'] = [
        '#type'       => 'container',
        '#attributes' => ['class' => ['file-settings']],
      ];
      $form['elements_container'][$sec_id][$q_id]['file_settings']['multiple'] = [
        '#type'          => 'checkbox',
        '#title'         => t('Allow multiple files'),
        '#default_value' => $is_multiple,
      ];
      $form['elements_container'][$sec_id][$q_id]['preview'] = [
        '#markup' => '<p class="field-preview">File upload field</p>',
      ];

      $r_id = 'rule_' . $q_id . '_is_multiple';
      $form_state['storage']['elements'][$r_id] = [
        'type'       => 'validation_rule',
        'parent'     => $q_id,
        'rule_type'  => 'is_multiple',
        'rule_value' => $is_multiple,
        'weight'     => 0,
      ];
      break;

    case 'radio':
    case 'checkbox':
    case 'dropdown':
    case 'list':
    case 'tag':
      _modularform_render_options($form, $form_state, $sec_id, $q_id, $q_type, $options, $val_rules, $is_multiple);
      break;
  }
}

function _modularform_render_options(&$form, &$form_state, $sec_id, $q_id, $q_type, $options, $val_rules, $is_multiple) {
  $question_options = array_filter($options, fn($el) => $el['parent'] === $q_id);

  foreach ($question_options as $o_id => $o_el) {
    $form['elements_container'][$sec_id][$q_id][$o_id] = [
      '#type'       => 'container',
      '#attributes' => ['class' => ['depth-option']],
    ];
    $form['elements_container'][$sec_id][$q_id][$o_id]['option_label'] = [
      '#type'          => 'textfield',
      '#title'         => t('Option'),
      '#default_value' => $o_el['option'] ?? '',
      '#required'      => TRUE,
    ];
    $form['elements_container'][$sec_id][$q_id][$o_id]['drag_handle'] = [
      '#markup' => '<div class="drag-handle" data-drag-scope="option" title="Drag to reorder"><span></span><span></span><span></span></div>',
      '#weight' => -99,
    ];
    $form['elements_container'][$sec_id][$q_id][$o_id]['weight'] = [
      '#type'          => 'hidden',
      '#default_value' => $o_el['weight'] ?? 0,
      '#attributes'    => ['class' => ['option-weight']],
    ];
    $form['elements_container'][$sec_id][$q_id][$o_id]['delete'] = [
      '#type'                    => 'submit',
      '#value'                   => t('Delete'),
      '#name'                    => 'delete_' . $o_id,
      '#submit'                  => ['modularform_delete_element_submit'],
      '#ajax'                    => [
        'callback' => 'modularform_ajax_question_callback',
        'wrapper'  => 'question-wrapper-' . $q_id,
      ],
      '#limit_validation_errors' => [],
    ];
  }

  if ($q_type === 'dropdown') {
    $form['elements_container'][$sec_id][$q_id]['dropdown_multiple'] = [
      '#type'          => 'checkbox',
      '#title'         => t('Allow multiple selections'),
      '#default_value' => $is_multiple,
    ];

    $r_id = 'rule_' . $q_id . '_is_multiple';
    $form_state['storage']['elements'][$r_id] = [
      'type'       => 'validation_rule',
      'parent'     => $q_id,
      'rule_type'  => 'is_multiple',
      'rule_value' => $is_multiple,
      'weight'     => 0,
    ];
  }

  $form['elements_container'][$sec_id][$q_id]['add_opt'] = [
    '#type'   => 'submit',
    '#value'  => t('+ Add Option'),
    '#name'   => 'add_opt_' . $q_id,
    '#submit' => ['modularform_add_element_submit'],
    '#ajax'   => [
      'callback' => 'modularform_ajax_question_callback',
      'wrapper'  => 'question-wrapper-' . $q_id,
    ],
  ];
}

function _modularform_render_question_validation(&$form, &$form_state, $sec_id, $q_id, $q_type, $q_el, $val_rules, $show_adv_val) {
  $form['elements_container'][$sec_id][$q_id]['validation'] = [
    '#type'       => 'container',
    '#attributes' => [
      'id'    => 'validation-wrapper-' . $q_id,
      'class' => ['question-validation-block'],
    ],
  ];

  $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'] = [
    '#type'       => 'container',
    '#attributes' => [
      'id'    => 'adv-validation-wrapper-' . $q_id,
      'class' => ['adv-validation-block'],
    ],
  ];

  if ($show_adv_val) {
    $q_val_rules = array_filter($val_rules, fn($el) => $el['parent'] === $q_id);
    _modularform_render_adv_rules($form, $sec_id, $q_id, $q_type, $q_val_rules);
  }

  $form['elements_container'][$sec_id][$q_id]['validation']['is_required'] = [
    '#type'          => 'checkbox',
    '#title'         => t('Required'),
    '#default_value' => $q_el['is_required'] ?? 0,
  ];

  $form['elements_container'][$sec_id][$q_id]['validation']['add_validation'] = [
    '#type'          => 'checkbox',
    '#title'         => t('Add validation rules'),
    '#default_value' => $show_adv_val ? 1 : 0,
    '#ajax'          => [
      'callback' => 'modularform_ajax_validation_callback',
      'wrapper'  => 'adv-validation-wrapper-' . $q_id,
      'event'    => 'change',
    ],
  ];
}

function _modularform_render_adv_rules(&$form, $sec_id, $q_id, $q_type, $q_val_rules) {
  foreach ($q_val_rules as $r_id => $r_el) {
    $current_rule_type = $r_el['rule_type'] ?? '';
    $value_type        = _modularform_rule_value_type($q_type, $current_rule_type);

    $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id] = [
      '#type'       => 'container',
      '#attributes' => ['class' => ['adv-rule-row']],
    ];

    $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_type'] = [
      '#type'          => 'select',
      '#title'         => t('Rule'),
      '#options'       => _modularform_validation_rule_options($q_type),
      '#default_value' => $current_rule_type,
      '#ajax'          => [
        'callback' => 'modularform_ajax_rule_value_callback',
        'wrapper'  => 'rule-value-wrapper-' . $r_id,
        'event'    => 'change',
      ],
    ];

    $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_value_wrapper'] = [
      '#type'       => 'container',
      '#attributes' => ['id' => 'rule-value-wrapper-' . $r_id],
    ];

    switch ($value_type) {
      case 'none':
        $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_value_wrapper']['rule_value'] = [
          '#markup' => '<p class="rule-no-value">' . t('No value required for this rule.') . '</p>',
        ];
        break;

      case 'number':
        $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_value_wrapper']['rule_value'] = [
          '#type'              => 'textfield',
          '#title'             => t('Value'),
          '#default_value'     => $r_el['rule_value'] ?? '',
          '#size'              => 10,
          '#element_validate'  => ['element_validate_number'],
        ];
        break;

      case 'select':
        $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_value_wrapper']['rule_value'] = [
          '#type'          => 'select',
          '#title'         => t('Value'),
          '#options'       => _modularform_rule_value_options($q_type, $current_rule_type),
          '#default_value' => $r_el['rule_value'] ?? '',
        ];
        break;

      case 'text':
      default:
        $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_value_wrapper']['rule_value'] = [
          '#type'          => 'textfield',
          '#title'         => t('Value'),
          '#default_value' => $r_el['rule_value'] ?? '',
        ];
        break;
    }

    $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['error_message'] = [
      '#type'          => 'textfield',
      '#title'         => t('Custom error message'),
      '#placeholder'   => t('Optional'),
      '#default_value' => $r_el['error_message'] ?? '',
    ];

    $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['delete'] = [
      '#type'                    => 'submit',
      '#value'                   => t('Delete'),
      '#name'                    => 'delete_' . $r_id,
      '#submit'                  => ['modularform_delete_element_submit'],
      '#ajax'                    => [
        'callback' => 'modularform_ajax_validation_callback',
        'wrapper'  => 'adv-validation-wrapper-' . $q_id,
      ],
      '#limit_validation_errors' => [],
      '#attributes'              => ['class' => ['btn-delete-red']],
    ];
  }

  $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules']['add_rule'] = [
    '#type'   => 'submit',
    '#value'  => t('+ Add Rule'),
    '#name'   => 'add_rule_' . $q_id,
    '#submit' => ['modularform_add_element_submit'],
    '#ajax'   => [
      'callback' => 'modularform_ajax_validation_callback',
      'wrapper'  => 'adv-validation-wrapper-' . $q_id,
    ],
  ];
}

// ── Utility ───────────────────────────────────────────────────────────────────

function _modularform_get_is_multiple($q_id, $val_rules) {
  foreach ($val_rules as $r_el) {
    if ($r_el['parent'] === $q_id && ($r_el['rule_type'] ?? '') === 'is_multiple') {
      return (int) ($r_el['rule_value'] ?? 0);
    }
  }
  return 0;
}

/**
 * add Elements. This is where the form ends
 */
function modularform_add_element_submit($form, &$form_state) {
  $trigger = $form_state['triggering_element']['#name'];
  $fid = $form_state['storage']['fid'];

  if ($trigger === 'add_section') {
    $new_id = 'sec_' . $fid . '_' . uniqid();
    $siblings = array_filter($form_state['storage']['elements'], fn($el) => $el['type'] === 'section');
    $sec_count = count($siblings);
    $form_state['storage']['elements'][$new_id] = [
      'type' => 'section',
      'title' => 'Section ' . ($sec_count + 1),
      'weight' => $siblings ? max(array_column($siblings, 'weight')) + 1 : 0,
      'parent' => NULL,
    ];
  }
  elseif (strpos($trigger, 'add_q_') === 0) {
    $new_id = 'q_' . $fid . '_' . uniqid();
    $parent_id = str_replace('add_q_', '', $trigger);
    $siblings = array_filter($form_state['storage']['elements'], fn($el) => $el['type'] === 'question' && $el['parent'] === $parent_id);
    $form_state['storage']['elements'][$new_id] = [
      'type' => 'question',
      'parent' => $parent_id,
      'question_type' => 'text',
      'weight' => $siblings ? max(array_column($siblings, 'weight')) + 1 : 0,
    ];
  }
  elseif (strpos($trigger, 'add_opt_') === 0) {
    $new_id = 'opt_' . $fid . '_' . uniqid();
    $parent_id = str_replace('add_opt_', '', $trigger);
    $siblings = array_filter($form_state['storage']['elements'], fn($el) => $el['type'] === 'option' && $el['parent'] === $parent_id);
    $form_state['storage']['elements'][$new_id] = [
      'type' => 'option',
      'parent' => $parent_id,
      'weight' => $siblings ? max(array_column($siblings, 'weight')) + 1 : 0,
    ];
  }
  elseif (strpos($trigger, 'add_rule_') === 0) {
    $new_id = 'r_' . $fid . '_' . uniqid();
    $q_id = substr($trigger, strlen('add_rule_'));
    $siblings = array_filter($form_state['storage']['elements'], fn($el) => $el['type'] === 'validation_rule' && $el['parent'] === $q_id);
    $form_state['storage']['elements'][$new_id] = [
      'type' => 'validation_rule',
      'parent' => $q_id,
      'rule_type' => '',
      'weight' => $siblings ? max(array_column($siblings, 'weight')) + 1 : 0,
    ];
  }

  $form_state['rebuild'] = TRUE;
}

/**
 * delete_elements
 */
function modularform_delete_element_submit($form, &$form_state) {
  $trigger = $form_state['triggering_element']['#name'];
  $target_id = str_replace('delete_', '', $trigger);
  $elements = &$form_state['storage']['elements'];



  // ✅ Save parent info BEFORE unset
  $form_state['storage']['last_deleted'] = [
    'id' => $target_id,
    'type' => $elements[$target_id]['type'],
    'parent' => $elements[$target_id]['parent'],
  ];

  $ids_to_remove = modularform_collect_descendants($target_id, $elements);
  $ids_to_remove[] = $target_id;

  foreach ($ids_to_remove as $id) {
    unset($elements[$id]);
  }

  $form_state['rebuild'] = TRUE;
}

/**
 * Recursively finds all descendant IDs of a given element.
 */
function modularform_collect_descendants($parent_id, $elements) {
  $descendants = [];
  foreach ($elements as $id => $el) {
    if ($el['parent'] === $parent_id) {
      $descendants[] = $id;
      // Recurse to catch grandchildren (e.g. options under a question)
      $descendants = array_merge(
        $descendants,
        modularform_collect_descendants($id, $elements)
      );
    }
  }
  return $descendants;
}

function modularform_get_delete_target($form_state) {
  $last = $form_state['storage']['last_deleted'];

  if ($last['type'] == 'question') {
    return [
      'type' => 'section',
      'sec_id' => $last['parent'],
    ];
  }

  if ($last['type'] == 'option') {
    $q_id = $last['parent'];
    return [
      'type' => 'question',
      'q_id' => $q_id,
      'sec_id' => $form_state['storage']['elements'][$q_id]['parent'],
    ];
  }
}

function modularform_ajax_top_callback($form, $form_state) {
  return $form['elements_container'];
}

function modularform_ajax_section_callback($form, $form_state) {
  $trigger = $form_state['triggering_element']['#name'];

  if (strpos($trigger, 'add_q_') === 0) {
    $sec_id = str_replace('add_q_', '', $trigger);
  }
  else {
    $target = modularform_get_delete_target($form_state); // ← no $elements arg
    $sec_id = $target['sec_id'];
  }

  return $form['elements_container'][$sec_id];
}

function modularform_ajax_question_callback($form, $form_state) {
  $trigger = $form_state['triggering_element']['#name'];
  $parents = $form_state['triggering_element']['#parents'];
  $elements = $form_state['storage']['elements'];

  if (strpos($trigger, 'add_opt_') === 0) {
    $q_id = str_replace('add_opt_', '', $trigger);
  }
  elseif (strpos($trigger, 'delete_') === 0) {
    $target = modularform_get_delete_target($form_state);
    $q_id = $target['q_id'];
  }
  else {
    $q_id = $parents[2];
  }

  $sec_id = $elements[$q_id]['parent'];
  return $form['elements_container'][$sec_id][$q_id];
}

function modularform_ajax_validation_callback($form, &$form_state) {
  $parents = $form_state['triggering_element']['#parents'];
  $sec_id = $parents[1];
  $q_id = $parents[2];

  return $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'];
}

function modularform_ajax_rule_value_callback($form, &$form_state) {
  $parents = $form_state['triggering_element']['#parents'];
  // parents: [elements_container, sec_id, q_id, validation, adv_rules, r_id, rule_type]
  $sec_id = $parents[1];
  $q_id = $parents[2];
  $r_id = $parents[5];

  return $form['elements_container'][$sec_id][$q_id]['validation']['adv_rules'][$r_id]['rule_value_wrapper'];
}

/**
 * Modularform submit function
 */
function modularform_builder_form_submit($form, &$form_state) {
  $fid    = $form_state['storage']['fid'];
  $is_new = $form_state['storage']['is_new'] ?? FALSE;

  // resolve once, cache in storage
  if (!isset($form_state['storage']['edit_state'])) {
    $form_state['storage']['edit_state'] = _modularform_resolve_edit_state($fid);
  }
  $rules = _modularform_edit_rules($form_state['storage']['edit_state']);
//  neatly prepares the data
  $records = _modularform_build_records($fid, $form_state);

  if ($is_new) {
    _modularform_create($fid, $records);
  } else {
    _modularform_edit($fid, $records);
  }

  db_update('modularform_forms')
    ->fields(['updated_at' => REQUEST_TIME])
    ->condition('id', $fid)
    ->execute();

  drupal_set_message(t('Survey saved successfully.'));
  $form_state['redirect'] = '/forms/' . $fid . '/builder/preview';
}

/**
 * @param $fid
 * @param $form_state
 *
 * @return array[]
 *
 * just to neatly wrap the input for db insert.
 */
function _modularform_build_records($fid, &$form_state) {
  $elements = $form_state['storage']['elements'];
  $values   = $form_state['values']['elements_container'];
  $now      = REQUEST_TIME;

  $sections  = [];
  $questions = [];
  $options   = [];
  $val_rules = [];

  foreach ($elements as $id => $el) {
    switch ($el['type']) {
      case 'section':
        if (isset($values[$id]['weight'])) {
          $el['weight'] = (int) $values[$id]['weight'];
        }
        $sections[$id] = $el;
        break;

      case 'question':
        $sec_id  = $el['parent'];
        $new_sec = $values[$sec_id][$id]['parent_sec'] ?? $sec_id;
        if (isset($values[$sec_id][$id]['weight'])) {
          $el['weight'] = (int) $values[$sec_id][$id]['weight'];
        }
        $el['parent']          = $new_sec;
        $el['original_parent'] = $sec_id;
        $questions[$id]        = $el;
        break;

      case 'option':
        $q_id   = $el['parent'];
        $sec_id = $elements[$q_id]['parent'] ?? NULL;
        if ($sec_id && isset($values[$sec_id][$q_id][$id]['weight'])) {
          $el['weight'] = (int) $values[$sec_id][$q_id][$id]['weight'];
        }
        $options[$id] = $el;
        break;

      case 'validation_rule':
        $val_rules[$id] = $el;
        break;
    }
  }

  uasort($sections,  fn($a, $b) => $a['weight'] <=> $b['weight']);
  uasort($questions, fn($a, $b) => $a['weight'] <=> $b['weight']);
  uasort($options,   fn($a, $b) => $a['weight'] <=> $b['weight']);
  uasort($val_rules, fn($a, $b) => $a['weight'] <=> $b['weight']);

  $sec_records = [];
  foreach ($sections as $sec_id => $sec_el) {
    $sec_records[$sec_id] = [
      'sec_id'     => $sec_id,
      'form_id'    => $fid,
      'name'       => $values[$sec_id]['title'] ?? $sec_el['title'] ?? '',
      'weight'     => $sec_el['weight'],
      'created_at' => $now,
      'updated_at' => $now,
    ];
  }

  $q_records        = [];
  $position_counter = [];
  foreach ($questions as $q_id => $q_el) {
    $sec_id   = $q_el['parent'];
    $orig_sec = $q_el['original_parent'] ?? $sec_id;
    $q_type   = $q_el['question_type'] ?? 'text';
    $q_vals   = $values[$orig_sec][$q_id] ?? [];
    $position = $position_counter[$sec_id] ?? 0;
    $position_counter[$sec_id] = $position + 1;

    $q_records[$q_id] = [
      'q_id'        => $q_id,
      'form_id'     => $fid,
      'sec_id'      => $sec_id,
      'label'       => $q_vals['question_text'] ?? '',
      'type'        => modularform_get_type_map($q_type) ?? 0,
      'datatype'    => modularform_get_datatype($q_type) ?? 'varchar',
      'is_required' => (int) ($q_vals['validation']['is_required'] ?? 0),
      'position'    => $position,
      'created_at'  => $now,
      'updated_at'  => $now,
    ];
  }

  $o_records = [];
  foreach ($options as $o_id => $o_el) {
    $q_id     = $o_el['parent'];
    $sec_id   = $questions[$q_id]['parent'] ?? NULL;
    $orig_sec = $questions[$q_id]['original_parent'] ?? $sec_id;

    $o_records[$o_id] = [
      'o_id'       => $o_id,
      'q_id'       => $q_id,
      'form_id'    => $fid,
      'option'     => $values[$orig_sec][$q_id][$o_id]['option_label'] ?? '',
      'weight'     => $o_el['weight'],
      'created_at' => $now,
      'updated_at' => $now,
    ];
  }

  $r_records = [];
  foreach ($val_rules as $r_id => $r_el) {
    $q_id     = $r_el['parent'];
    $sec_id   = $questions[$q_id]['parent'] ?? NULL;
    $orig_sec = $questions[$q_id]['original_parent'] ?? $sec_id;
    $q_type   = $questions[$q_id]['question_type'] ?? '';

    if (str_ends_with($r_id, '_is_multiple')) {
      $is_multiple = match ($q_type) {
        'dropdown' => (int) ($values[$orig_sec][$q_id]['dropdown_multiple'] ?? 0),
        'file'     => (int) ($values[$orig_sec][$q_id]['file_settings']['multiple'] ?? 0),
        default    => 0,
      };
      $r_records[$r_id] = [
        'r_id'          => $r_id,
        'q_id'          => $q_id,
        'form_id'       => $fid,
        'rule_type'     => 'is_multiple',
        'rule_value'    => $is_multiple,
        'error_message' => '',
        'weight'        => 0,
        'created_at'    => $now,
        'updated_at'    => $now,
      ];
      continue;
    }

    $r_vals = $values[$orig_sec][$q_id]['validation']['adv_rules'][$r_id] ?? [];
    $r_records[$r_id] = [
      'r_id'          => $r_id,
      'q_id'          => $q_id,
      'form_id'       => $fid,
      'rule_type'     => $r_vals['rule_type'] ?? '',
      'rule_value'    => $r_vals['rule_value_wrapper']['rule_value'] ?? '',
      'error_message' => $r_vals['error_message'] ?? '',
      'weight'        => $r_el['weight'],
      'created_at'    => $now,
      'updated_at'    => $now,
    ];
  }

  return [
    'sections'         => $sec_records,
    'questions'        => $q_records,
    'options'          => $o_records,
    'validation_rules' => $r_records,
  ];
}

function _modularform_create($fid, array $records) {
  foreach ($records['sections'] as $row) {
    db_insert('modularform_sections')->fields($row)->execute();
  }
  foreach ($records['questions'] as $row) {
    db_insert('modularform_questions')->fields($row)->execute();
  }
  foreach ($records['options'] as $row) {
    db_insert('modularform_question_options')->fields($row)->execute();
  }
  foreach ($records['validation_rules'] as $row) {
    db_insert('modularform_validation_rules')->fields($row)->execute();
  }
}

function _modularform_edit($fid, array $records) {
  $live_sec_ids = array_keys($records['sections'])         ?: [''];
  $live_q_ids   = array_keys($records['questions'])        ?: [''];
  $live_o_ids   = array_keys($records['options'])          ?: [''];
  $live_r_ids   = array_keys($records['validation_rules']) ?: [''];

  // Delete orphans — children before parents
  db_delete('modularform_validation_rules')
    ->condition('form_id', $fid)
    ->condition('r_id', $live_r_ids, 'NOT IN')
    ->execute();
  db_delete('modularform_question_options')
    ->condition('form_id', $fid)
    ->condition('o_id', $live_o_ids, 'NOT IN')
    ->execute();
  db_delete('modularform_questions')
    ->condition('form_id', $fid)
    ->condition('q_id', $live_q_ids, 'NOT IN')
    ->execute();
  db_delete('modularform_sections')
    ->condition('form_id', $fid)
    ->condition('sec_id', $live_sec_ids, 'NOT IN')
    ->execute();

  // Upsert survivors
  _modularform_bulk_upsert('modularform_sections', array_values($records['sections']), [
    'name', 'weight', 'updated_at',
  ]);
  _modularform_bulk_upsert('modularform_questions', array_values($records['questions']), [
    'sec_id', 'label', 'type', 'datatype', 'is_required', 'position', 'updated_at',
  ]);
  _modularform_bulk_upsert('modularform_question_options', array_values($records['options']), [
    'option', 'weight', 'updated_at',
  ]);
  _modularform_bulk_upsert('modularform_validation_rules', array_values($records['validation_rules']), [
    'rule_type', 'rule_value', 'error_message', 'weight', 'updated_at',
  ]);
}

function _modularform_bulk_upsert($table, array $records, array $update_cols) {
  if (empty($records)) {
    return;
  }
  $connection = Database::getConnection();
  $prefixed   = $connection->prefixTables('{' . $table . '}');
  $cols       = array_keys($records[0]);
  $col_list   = implode(', ', $cols);
  $row_ph     = '(' . implode(', ', array_fill(0, count($cols), '?')) . ')';
  $rows_sql   = implode(', ', array_fill(0, count($records), $row_ph));
  $update_parts = array_map(fn($c) => "$c = VALUES($c)", $update_cols);

  $sql = "INSERT INTO $prefixed ($col_list) VALUES $rows_sql"
    . " ON DUPLICATE KEY UPDATE " . implode(', ', $update_parts);

  $args = [];
  foreach ($records as $row) {
    foreach ($cols as $col) {
      $args[] = $row[$col];
    }
  }

  $stmt = $connection->prepare($sql);
  $stmt->execute($args);
}

function _modularform_resolve_edit_state($fid) {
  $form_status = db_select('modularform_forms', 'f')
    ->fields('f', ['status'])
    ->condition('id', $fid)
    ->execute()
    ->fetchField();

  if ((int) $form_status === 0) {
    return 'draft';
  }

  $has_responses = (bool) db_select('modularform_responses', 'r')
    ->condition('form_id', $fid)
    ->countQuery()
    ->execute()
    ->fetchField();

  return $has_responses ? 'published_has_responses' : 'published_no_responses';
}
