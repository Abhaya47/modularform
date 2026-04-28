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
