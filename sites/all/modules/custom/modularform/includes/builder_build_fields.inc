<?php

/**
 * @file
 * Shared form field builder. Shared between preview, submit.
 */
function _modularform_build_fields(&$form, $elements, $mode = 'preview', $defaults = []) {
  $form['#attributes']['class'][] = 'modularform-preview-form';
  if (empty($elements)) {
    $form['empty'] = ['#markup' => '<p>' . t('No form elements found.') . '</p>'];
    return $form;
  }

  $sections = array_filter($elements, fn($el) => $el['type'] === 'section');
  $questions = array_filter($elements, fn($el) => $el['type'] === 'question');
  $options = array_filter($elements, fn($el) => $el['type'] === 'option');
  $val_rules = array_filter($elements, fn($el) => $el['type'] === 'validation_rule');

  uasort($sections, fn($a, $b) => $a['weight'] <=> $b['weight']);
  uasort($questions, fn($a, $b) => $a['weight'] <=> $b['weight']);

  // Derive file_multiple and dropdown_multiple from validation rules
  foreach ($questions as $q_id => &$q) {
    $q_rules = array_filter($val_rules, fn($r) => $r['parent'] === $q_id);
    if ($q['question_type'] === 'file') {
      foreach ($q_rules as $r) {
        if ($r['rule_type'] === 'max_files' && (int) $r['rule_value'] > 1) {
          $q['file_multiple'] = TRUE;
        }
      }
    }
    if ($q['question_type'] === 'dropdown') {
      foreach ($q_rules as $r) {
        if ($r['rule_type'] === 'max_selections' && (int) $r['rule_value'] > 1) {
          $q['dropdown_multiple'] = TRUE;
        }
      }
    }
  }
  unset($q);

  // Attach Select2 library and our preview JS/CSS
  $form['#attached']['library'][] = ['modularform', 'select2'];
  $form['#attached']['js'][] = drupal_get_path('module', 'modularform') . '/js/modularform.preview.js';
  $form['#attached']['css'][] = drupal_get_path('module', 'modularform') . '/css/modularform.preview.css';

  // Pass validation rules to JS for client-side validation
  $js_rules = [];
  foreach ($questions as $q_id => $q) {
    $q_rules = array_filter($val_rules, fn($r) => $r['parent'] === $q_id);
    if (!empty($q_rules)) {
      foreach ($q_rules as $r) {
        $js_rules[$q_id][] = [
          'rule_type' => $r['rule_type'],
          'rule_value' => $r['rule_value'],
          'error_message' => $r['error_message'] ?? '',
          'q_type' => $q['question_type'],
        ];
      }
    }
  }
  $form['#attached']['js'][] = [
    'data' => ['modularformPreview' => ['rules' => $js_rules]],
    'type' => 'setting',
  ];

  $form['#tree'] = TRUE;
  $form['#attributes']['enctype'] = 'multipart/form-data';

  $is_anonymous_form = !empty($form_record['anonymous_responses']);
  if (!$is_anonymous_form) {
    global $user;
    if (!$user->uid) {
      $form['_guest_email'] = [
        '#type' => 'textfield',
        '#title' => t('Your Email'),
        '#required' => TRUE,
        '#maxlength' => 255,
        '#weight' => -1000,
        '#attributes' => [
          'type' => 'email',
          'placeholder' => t('Enter your email address'),
        ],
        '#prefix' => '<div class="preview-section preview-guest-email">',
        '#suffix' => '</div>',
      ];
    }
  }

  foreach ($sections as $sec_id => $sec) {
    $form[$sec_id] = [
      '#type' => 'container',
      '#attributes' => ['class' => ['preview-section']],
    ];

    $form[$sec_id]['_title'] = [
      '#markup' => '<h3 class="preview-section-title">' . check_plain($sec['title']) . '</h3>',
    ];

    $sec_questions = array_filter($questions, fn($q) => $q['parent'] === $sec_id);
    uasort($sec_questions, fn($a, $b) => $a['weight'] <=> $b['weight']);

    foreach ($sec_questions as $q_id => $q) {
      $q_type = $q['question_type'] ?? 'text';
      $label = $q['label'] ?? '';
      $required = !empty($q['is_required']);

      $q_options = array_filter($options, fn($o) => $o['parent'] === $q_id);
      uasort($q_options, fn($a, $b) => $a['weight'] <=> $b['weight']);
      $options_list = [];
      foreach ($q_options as $o_id => $o) {
        $options_list[$o_id] = check_plain($o['option']); // key = o_id, value = label
      }

      $form[$sec_id][$q_id] = [
        '#type' => 'container',
        '#attributes' => [
          'class' => ['preview-question'],
          'data-q-id' => $q_id,
          'data-q-type' => $q_type,
        ],
      ];

      switch ($q_type) {
        case 'text':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'         => 'textfield',
            '#title'        => $label,
            '#required'     => $required,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'   => ['data-q-id' => $q_id],
          ];
          break;

        case 'textarea':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'textarea',
            '#title'         => $label,
            '#rows'          => 5,
            '#required'      => $required,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'    => ['data-q-id' => $q_id],
          ];
          break;

        case 'richtext':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'textarea',
            '#title'         => $label,
            '#required'      => $required,
            '#rows'          => 8,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'    => [
              'class' => ['dfp-text-editor-area'],
              'id'    => 'editor-' . $q_id,
            ],
            '#after_build' => ['_dfp_attach_tinymce'],
          ];
          break;

        case 'date':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'date',
            '#title'         => $label,
            '#required'      => $required,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'    => ['data-q-id' => $q_id],
          ];
          break;

        case 'time':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'textfield',
            '#title'         => $label,
            '#required'      => $required,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'    => ['type' => 'time', 'data-q-id' => $q_id],
          ];
          break;

        case 'boolean':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'radios',
            '#title'         => $label,
            '#options'       => ['yes' => t('Yes'), 'no' => t('No')],
            '#required'      => $required,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'    => ['data-q-id' => $q_id],
          ];
          break;

        case 'radio':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'radios',
            '#title'         => $label,
            '#options'       => $options_list,
            '#required'      => $required,
            '#default_value' => $defaults[$q_id] ?? NULL,
            '#attributes'    => ['data-q-id' => $q_id],
          ];
          break;

        case 'list':
        case 'checkbox':
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'checkboxes',
            '#title'         => $label,
            '#options'       => $options_list,
            '#required'      => $required,
            // checkboxes expects an array of checked keys
            '#default_value' => $defaults[$q_id] ?? [],
            '#attributes'    => ['data-q-id' => $q_id],
          ];
          break;

        case 'dropdown':
          $is_multiple = !empty($q['dropdown_multiple']);
          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'select',
            '#title'         => $label,
            '#options'       => $options_list,
            '#empty_option'  => $is_multiple ? NULL : t('- Select -'),
            '#required'      => $required,
            '#multiple'      => $is_multiple,
            // multiple select expects an array too
            '#default_value' => $defaults[$q_id] ?? ($is_multiple ? [] : NULL),
            '#attributes'    => [
              'data-q-id' => $q_id,
              'class'     => ['select2-enable'],
            ],
          ];
          break;

        case 'tag':
          $default_tags = $defaults[$q_id] ?? [];   // already [{id, text}, ...]
          $default_ids  = array_column((array) $default_tags, 'id');

          $form[$sec_id][$q_id]['answer'] = [
            '#type'          => 'textfield',
            '#title'         => $label,
            '#required'      => $required,
            '#default_value' => implode(',', $default_ids),
            '#attributes'    => [
              'data-q-id'   => $q_id,
              'data-ac-url' => url('modularform/tags/autocomplete'),
              'class'       => ['select2-tags-hidden'],
            ],
          ];

          drupal_add_js(
            ['modularformDefaultTags' => [$q_id => $default_tags]],
            ['type' => 'setting']
          );
          drupal_add_js(drupal_get_path('module', 'modularform') . '/js/modularform.preview.js');
          break;

        case 'file':
          // Files can't be pre-populated in managed_file, skip default
          $is_multiple = !empty($q['file_multiple']);
          $form[$sec_id][$q_id]['answer'] = [
            '#type'             => 'managed_file',
            '#title'            => $label,
            '#required'         => $required,
            '#upload_location'  => 'temporary://modularform_preview',
            '#upload_validators' => ['file_validate_size' => [25 * 1024 * 1024]],
            '#multiple'         => $is_multiple,
            '#description'      => t('Preview only — file will not be saved.'),
            '#attributes'       => ['data-q-id' => $q_id],
          ];
          break;
      }
      if ($mode === 'view') {
        if (isset($form[$sec_id][$q_id]['answer'])) {
          $form[$sec_id][$q_id]['answer']['#disabled'] = TRUE;
          $form[$sec_id][$q_id]['answer']['#attributes']['class'][] = 'answer-readonly';
        }
      }
    }
  }
}

function _dynamic_form_preview_maybe_load_tinymce($form_id) {
  $tinymce_js = 'sites/all/libraries/tinymce/js/tinymce/tinymce.min.js';
  if (!file_exists(DRUPAL_ROOT . '/' . $tinymce_js)) {
    return;
  }
  drupal_add_js($tinymce_js, ['type' => 'file', 'weight' => 10]);
  $init_js = <<<JS
(function ($) {
  Drupal.behaviors.dfpTinymcePreview = {
    attach: function (context, settings) {
      \$('.dfp-text-editor-area', context).once('dfp-tinymce-init', function () {
        if (typeof tinymce === 'undefined') { return; }
        tinymce.init({
          selector  : '#' + this.id,
          readonly  : false,
          height    : 200,
          menubar   : false,
          statusbar : false,
          plugins   : 'lists link',
          toolbar   : 'bold italic underline strikethrough | bullist numlist blockquote | link'
        });
      });
    }
  };
})(jQuery);
JS;
  drupal_add_js($init_js, [
    'type' => 'inline',
    'scope' => 'footer',
    'weight' => 20,
  ]);
}

/**
 * #after_build callback to trigger the TinyMCE inclusion.
 */
function _dfp_attach_tinymce($element, &$form_state) {
  // Grab the form ID from the form state if needed by your helper
  $form_id = isset($form_state['build_info']['form_id']) ? $form_state['build_info']['form_id'] : 'unknown_form_id';

  // Call your custom helper function
  _dynamic_form_preview_maybe_load_tinymce($form_id);

  // #after_build callbacks MUST return the element
  return $element;
}

function modularform_tags_autocomplete() {
  // Resolve IDs to labels for initSelection
  if (!empty($_GET['ids'])) {
    $id_list = array_filter(array_map('trim', explode(',', $_GET['ids'])));
    $rows = db_select('modularform_tags', 't')
      ->fields('t', ['id', 'title'])
      ->condition('t.id', $id_list, 'IN')
      ->execute()
      ->fetchAllAssoc('id');
    $results = [];
    foreach ($id_list as $id) {
      if (isset($rows[$id])) {
        $results[] = ['id' => $id, 'text' => $rows[$id]->title];
      }
    }
    drupal_json_output(['results' => $results, 'more' => FALSE]);
    drupal_exit();
  }

  // Existing search code unchanged
  $search = isset($_GET['term']) ? check_plain(trim($_GET['term'])) : '';
  $limit  = isset($_GET['page_limit']) ? (int) $_GET['page_limit'] : 20;
  $results = [];
  $query = db_select('modularform_tags', 't')
    ->fields('t', ['id', 'title']);
  if ($search !== '') {
    $query->condition('t.title', '%' . db_like($search) . '%', 'LIKE');
  }
  foreach ($query->orderBy('t.title')->range(0, $limit)->execute() as $row) {
    $results[] = ['id' => $row->id, 'text' => $row->title];
  }
  drupal_json_output(['results' => $results, 'more' => FALSE]);
  drupal_exit();
}

