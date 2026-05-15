<?php

/**
 * Returns full validation rule definitions keyed by question type.
 *
 * Each rule entry:
 *   'label'       => Human-readable name shown in the select.
 *   'value_type'  => 'none' | 'text' | 'number' | 'select'
 *   'value_options' => (only when value_type = 'select') keyed array of
 * choices.
 */
function _modularform_rule_definitions() {
  return [

    'text' => [
      'min_length' => [
        'label' => t('Min length (chars)'),
        'value_type' => 'number',
      ],
      'max_length' => [
        'label' => t('Max length (chars)'),
        'value_type' => 'number',
      ],
      'exact_length' => [
        'label' => t('Exact length (chars)'),
        'value_type' => 'number',
      ],
      'is_email' => ['label' => t('Valid email'), 'value_type' => 'none'],
      'is_url' => ['label' => t('Valid URL'), 'value_type' => 'none'],
      'is_numeric' => ['label' => t('Numeric only'), 'value_type' => 'none'],
      'is_alpha' => ['label' => t('Letters only'), 'value_type' => 'none'],
      'starts_with' => ['label' => t('Starts with'), 'value_type' => 'text'],
      'ends_with' => ['label' => t('Ends with'), 'value_type' => 'text'],
      'contains' => ['label' => t('Contains'), 'value_type' => 'text'],
      'regex' => ['label' => t('Matches regex'), 'value_type' => 'text'],
    ],

    'textarea' => [
      'min_length' => [
        'label' => t('Min length (chars)'),
        'value_type' => 'number',
      ],
      'max_length' => [
        'label' => t('Max length (chars)'),
        'value_type' => 'number',
      ],
      'min_words' => ['label' => t('Min word count'), 'value_type' => 'number'],
      'max_words' => ['label' => t('Max word count'), 'value_type' => 'number'],
      'contains' => ['label' => t('Contains phrase'), 'value_type' => 'text'],
      'regex' => ['label' => t('Matches regex'), 'value_type' => 'text'],
    ],

    'richtext' => [
      'min_words' => ['label' => t('Min word count'), 'value_type' => 'number'],
      'max_words' => ['label' => t('Max word count'), 'value_type' => 'number'],
      'contains' => ['label' => t('Contains phrase'), 'value_type' => 'text'],
    ],

    'date' => [
      'date_min' => ['label' => t('Not before date'), 'value_type' => 'text'],
      'date_max' => ['label' => t('Not after date'), 'value_type' => 'text'],
      'date_format' => [
        'label' => t('Required format'),
        'value_type' => 'text',
      ],
    ],

    'time' => [
      'time_min' => ['label' => t('Not before time'), 'value_type' => 'text'],
      'time_max' => ['label' => t('Not after time'), 'value_type' => 'text'],
    ],

    'list' => [
      'min_selections' => [
        'label' => t('Min selections'),
        'value_type' => 'number',
      ],
      'max_selections' => [
        'label' => t('Max selections'),
        'value_type' => 'number',
      ],
      'disallow_value' => [
        'label' => t('Disallow specific option'),
        'value_type' => 'text',
      ],
    ],

    'radio' => [
      'disallow_value' => [
        'label' => t('Disallow specific option'),
        'value_type' => 'text',
      ],
    ],

    'boolean' => [
      'must_be_true' => ['label' => t('Must be Yes'), 'value_type' => 'none'],
      'must_be_false' => ['label' => t('Must be No'), 'value_type' => 'none'],
    ],

    'checkbox' => [
      'min_selections' => [
        'label' => t('Min selections'),
        'value_type' => 'number',
      ],
      'max_selections' => [
        'label' => t('Max selections'),
        'value_type' => 'number',
      ],
      'exact_count' => [
        'label' => t('Exact number of selections'),
        'value_type' => 'number',
      ],
    ],

    'dropdown' => [
      'min_selections' => [
        'label' => t('Min selections'),
        'value_type' => 'number',
      ],
      'max_selections' => [
        'label' => t('Max selections'),
        'value_type' => 'number',
      ],
      'disallow_value' => [
        'label' => t('Disallow specific value'),
        'value_type' => 'text',
      ],
    ],

    'file' => [
      'max_filesize' => [
        'label' => t('Max file size (MB)'),
        'value_type' => 'number',
      ],
      'min_files' => [
        'label' => t('Min number of files'),
        'value_type' => 'number',
      ],
      'max_files' => [
        'label' => t('Max number of files'),
        'value_type' => 'number',
      ],
      'allowed_types' => [
        'label' => t('Allowed file types'),
        'value_type' => 'select',
        'value_options' => [
          'image' => t('Images (jpg, png, gif, webp)'),
          'document' => t('Documents (pdf, doc, docx, odt)'),
          'video' => t('Video (mp4, mov, avi)'),
          'audio' => t('Audio (mp3, wav, ogg)'),
          'archive' => t('Archives (zip, tar, gz)'),
          'any' => t('Any file type'),
        ],
      ],
    ],

    'tag' => [
      'min_tags' => ['label' => t('Min tags'), 'value_type' => 'number'],
      'max_tags' => ['label' => t('Max tags'), 'value_type' => 'number'],
      'exact_tags' => [
        'label' => t('Exact number of tags'),
        'value_type' => 'number',
      ],
    ],
  ];
}

/**
 * Returns just the labels — used to populate the rule_type select.
 */
function _modularform_validation_rule_options($q_type) {
  $defs = _modularform_rule_definitions();
  $type_rules = $defs[$q_type] ?? [];
  return array_map(fn($r) => $r['label'], $type_rules);
}

/**
 * Returns the value_type for a specific rule — used to render the right input.
 */
function _modularform_rule_value_type($q_type, $rule_type) {
  $defs = _modularform_rule_definitions();
  return $defs[$q_type][$rule_type]['value_type'] ?? 'text';
}

/**
 * Returns select options for a specific rule (only for value_type = 'select').
 */
function _modularform_rule_value_options($q_type, $rule_type) {
  $defs = _modularform_rule_definitions();
  return $defs[$q_type][$rule_type]['value_options'] ?? [];
}

/**
 * Returns edit permissions for a form based on its state.
 *
 * States:
 *   'draft'                   — no restrictions
 *   'published_no_responses'  — no restrictions
 *   'published_has_responses' — structural changes blocked
 */
function _modularform_edit_rules($state) {
  $rules = [
    'draft' => [
      'add_section' => TRUE,
      'delete_section' => TRUE,
      'add_question' => TRUE,
      'delete_question' => TRUE,
      'change_type' => TRUE,
      'rename_label' => TRUE,
      'reorder' => TRUE,
      'edit_options' => TRUE,
      'add_option' => TRUE,
      'delete_option' => TRUE,
      'change_required' => TRUE,
    ],

    'published_no_responses' => [
      'add_section' => TRUE,
      'delete_section' => TRUE,
      'add_question' => TRUE,
      'delete_question' => TRUE,
      'change_type' => TRUE,
      'rename_label' => TRUE,
      'reorder' => TRUE,
      'edit_options' => TRUE,
      'add_option' => TRUE,
      'delete_option' => TRUE,
      'change_required' => TRUE,
    ],

    'published_has_responses' => [
      'add_section' => FALSE,
      'delete_section' => FALSE,
      'add_question' => TRUE,   // optional only — enforced separately
      'delete_question' => FALSE,
      'change_type' => FALSE,
      'rename_label' => TRUE,   // warn in UI but allow
      'reorder' => TRUE,
      'edit_options' => FALSE,
      'add_option' => FALSE,
      'delete_option' => FALSE,
      'change_required' => FALSE,
    ],
  ];

  return $rules[$state] ?? $rules['draft'];
}

function modularform_type_map(): array {
  return [
    'text' => 0,
    'textarea' => 1,
    'radio' => 2,
    'boolean' => 3,
    'checkbox' => 4,
    'file' => 5,
    'dropdown' => 6,
    'tag' => 7,
    'richtext' => 8,
    'date' => 9,
    'time' => 10,
    'list' => 11,
  ];
}

function modularform_datatype_map(): array {
  return [
    'text' => 'varchar',
    'textarea' => 'text',
    'richtext' => 'text',
    'radio' => 'varchar',
    //#TODO change the value to int later
    'boolean' => 'bool',
    'checkbox' => 'varchar',
    // multi → child table //#TODO change the value to int later
    'tag' => 'int',
    // multi → child table
    'list' => 'varchar',
    // multi → child table //#TODO change the value to int later
    'dropdown' => 'varchar',
    //#TODO change the value to int later
    'file' => 'varchar',
    'date' => 'date',
    'time' => 'varchar',
  ];
}

function modularform_get_type_map(string $type): int {
  return modularform_type_map()[$type] ?? 0;
}

function modularform_get_type_int_map(int $type): string {
  $type_map = array_flip(modularform_type_map());

  return $type_map[$type] ?? 'text';
}

function modularform_get_datatype(string $q_type): string {
  return modularform_datatype_map()[$q_type] ?? 'varchar';
}

function modularform_visibility_get_int_map() {
  return [
    'public' => 0,
    'restricted' => 1,
    'private' => 2,
  ];
}

function modularform_visibility_get_text_map() {
  return array_flip(modularform_visibility_get_int_map());
  // result: [0 => 'public', 1 => 'restricted', 2 => 'private']
}

function solr_field_map() {
  static $map;

  if (!isset($map)) {
    $map = [
      'varchar' => 'ss_val',
      'text' => 'ts_val',
      'int' => 'is_val',
      'bool' => 'bs_val',
      'date' => 'ds_val',
    ];
  }

  return $map;
}

function value_col_map() {
  static $map;
  if (!isset($map)) {
    return [
      'varchar' => 'value_varchar',
      'text' => 'value_text',
      'int' => 'value_int',
      'bool' => 'value_bool',
      'date' => 'value_date',
    ];
  }
  return $map;
}
