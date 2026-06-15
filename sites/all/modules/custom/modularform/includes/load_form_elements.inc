<?php

module_load_include('inc', 'modularform', 'modularform.rules');

/**
 * Loads ordered questions for a form, keyed by q_id.
 *
 * Retrieves elements via _modularform_load_elements() and filters to
 * questions only, sorted by section weight then question weight — matching
 * the order a respondent sees in the form.
 *
 * @param int $fid
 *   The form ID.
 *
 * @return array
 *   Associative array keyed by q_id, ordered by section then question weight:
 *   @code
 *   [
 *     $q_id => [
 *       'label'         => string,
 *       'question_type' => string,
 *     ],
 *     ...
 *   ]
 *   @endcode
 *   Returns an empty array if the form has no elements or questions.
 */
function _modularform_load_elements($fid): array {
  $scalar = !is_array($fid);
  $fids   = array_values(array_unique(array_map('intval', (array) $fid)));

  if (empty($fids)) {
    return $scalar ? [] : [];
  }

  // --- Cache check ---
  // For scalar calls this is a cheap single lookup, same as before.
  // For array calls we check all, then only query what's missing.
  $results    = [];
  $uncached   = [];

  foreach ($fids as $f) {
    $cid   = 'modularform:elements:' . $f;
    $cache = cache_get($cid, 'cache_modularform');
    if ($cache && isset($cache->data)) {
      $results[$f] = $cache->data;
    }
    else {
      $uncached[] = $f;
      $results[$f] = []; // placeholder
    }
  }

  if (!empty($uncached)) {
    // --- 4 bulk IN queries for all uncached fids ---

    // Sections
    $sections = db_select('modularform_sections', 's')
      ->fields('s', ['id', 'sec_id', 'form_id', 'name', 'weight'])
      ->condition('form_id', $uncached, 'IN')
      ->orderBy('form_id')
      ->orderBy('weight')
      ->execute()
      ->fetchAllAssoc('sec_id', PDO::FETCH_ASSOC);

    // Questions
    $questions = db_select('modularform_questions', 'q')
      ->fields('q', [
        'id',
        'q_id',
        'form_id',
        'sec_id',
        'label',
        'type',
        'is_required',
        'position',
        'is_filterable',
      ])
      ->condition('form_id', $uncached, 'IN')
      ->orderBy('form_id')
      ->orderBy('sec_id')
      ->orderBy('position')
      ->execute()
      ->fetchAllAssoc('q_id', PDO::FETCH_ASSOC);

    // Options
    $options = db_select('modularform_question_options', 'o')
      ->fields('o', ['id', 'o_id', 'form_id', 'q_id', 'option', 'weight'])
      ->condition('form_id', $uncached, 'IN')
      ->orderBy('form_id')
      ->orderBy('q_id')
      ->orderBy('weight')
      ->execute()
      ->fetchAllAssoc('o_id', PDO::FETCH_ASSOC);

    // Validation rules
    $rules = db_select('modularform_validation_rules', 'r')
      ->fields('r', [
        'id',
        'r_id',
        'form_id',
        'q_id',
        'rule_type',
        'rule_value',
        'error_message',
        'weight',
      ])
      ->condition('form_id', $uncached, 'IN')
      ->orderBy('form_id')
      ->orderBy('q_id')
      ->orderBy('weight')
      ->execute()
      ->fetchAllAssoc('r_id', PDO::FETCH_ASSOC);

    // --- Split into per-form buckets ---

    // Sections
    foreach ($sections as $sec_id => $row) {
      $f = (int) $row['form_id'];
      $results[$f][$sec_id] = [
        'type' => 'section',
        'id' => (int) $row['id'],
        'title' => $row['name'],
        'weight' => (int) $row['weight'],
        'parent' => NULL,
      ];
    }

    // Questions
    foreach ($questions as $q_id => $row) {
      $f = (int) $row['form_id'];
      $results[$f][$q_id] = [
        'id' => (int) $row['id'],
        'type' => 'question',
        'parent' => $row['sec_id'],
        'label' => $row['label'],
        'question_type' => modularform_get_type_int_map($row['type']),
        'is_required' => (bool) $row['is_required'],
        'is_filterable'  => (bool) $row['is_filterable'],
        'add_validation' => FALSE,
        'weight' => (int) $row['position'],
      ];
    }

    // Options
    foreach ($options as $o_id => $row) {
      $f = (int) $row['form_id'];
      $results[$f][$o_id] = [
        'id' => (int) $row['id'],
        'type' => 'option',
        'parent' => $row['q_id'],
        'option' => $row['option'],
        'weight' => (int) $row['weight'],
      ];
    }

    // Validation rules — also flag parent question
    foreach ($rules as $r_id => $row) {
      $f = (int) $row['form_id'];
      if (isset($results[$f][$row['q_id']])) {
        $results[$f][$row['q_id']]['add_validation'] = TRUE;
      }
      $results[$f][$r_id] = [
        'type' => 'validation_rule',
        'parent' => $row['q_id'],
        'rule_type' => $row['rule_type'],
        'rule_value' => $row['rule_value'],
        'error_message' => $row['error_message'],
        'weight' => (int) $row['weight'],
      ];
    }

    // --- Write per-form cache entries ---
    foreach ($uncached as $f) {
      if (!empty($results[$f])) {
        cache_set('modularform:elements:' . $f, $results[$f], 'cache_modularform', CACHE_PERMANENT);
      }
    }
  }

  return $scalar ? ($results[$fids[0]] ?? []) : $results;
}
