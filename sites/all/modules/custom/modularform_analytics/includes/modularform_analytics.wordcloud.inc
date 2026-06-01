<?php

/**
 * @file
 * modularform_analytics.wordcloud.inc
 *
 * Page callback and data query for the tag word cloud.
 */

/**
 * Page callback: renders the tag word cloud page.
 */
function modularform_analytics_wordcloud_page() {
  $words = modularform_analytics_get_tag_counts();

  if (empty($words)) {
    return ['#markup' => t('No tag data available yet.')];
  }

  drupal_add_library('modularform_analytics', 'wordcloud');
  drupal_add_js(
    ['modularformWordCloud' => ['words' => $words]],
    'setting'
  );

  return theme('modularform_analytics_wordcloud');
}

/**
 * Query tag usage counts.
 *
 * Join chain:
 *   modularform_answers      (datatype = 7(tag))
 *   → modularform_answers_multi  (answer_id = answers.id)
 *   → modularform_tags           (id = answers_multi.value)
 *
 * @return array
 *   Array of ['text' => string, 'size' => int] sorted by count desc.
 */
function modularform_analytics_get_tag_counts($form_id = NULL) {
  $query = db_select('modularform_answers', 'a');
  $query->condition('a.datatype', 7);
  $query->join('modularform_answers_multi', 'am', 'am.answer_id = a.id');
  $query->join('modularform_tags', 't', 't.id = am.value');
  $query->join('modularform_responses', 's', 's.id = a.response_id');
  $query->join('modularform_forms', 'f', 'f.id = s.form_id');
  $query->join('modularform_questions', 'q', 'q.q_id = a.q_id AND q.form_id = f.id');

  // ← Filter by form if provided
  if ($form_id) {
    $query->condition('f.id', (int) $form_id);
  }

  $query->addField('t', 'title', 'tag_title');
  $query->addField('f', 'id', 'form_id');
  $query->addField('f', 'title', 'form_title');
  $query->addField('q', 'q_id', 'q_id');
  $query->addField('q', 'label', 'q_label');
  $query->addField('a', 'datatype', 'datatype');
  $query->addExpression('COUNT(am.id)', 'count');
  $query->groupBy('t.id');
  $query->groupBy('t.title');
  $query->groupBy('f.id');
  $query->groupBy('f.title');
  $query->groupBy('q.q_id');
  $query->groupBy('q.label');
  $query->groupBy('a.datatype');
  $query->orderBy('count', 'DESC');
  $rows = $query->execute()->fetchAll();

  if (empty($rows)) { return []; }

  // Group into tag → form → question hierarchy.
  $tags = [];
  foreach ($rows as $row) {
    $solr_key = modularform_build_solr_key($row->datatype, $row->q_id);
    if (!$solr_key) { continue; }

    $tid = $row->tag_title; // use title as key since that's what we filter by
    if (!isset($tags[$tid])) {
      $tags[$tid] = ['text' => check_plain($row->tag_title), 'count' => 0, 'sources' => []];
    }
    $tags[$tid]['count'] += (int) $row->count;
    $tags[$tid]['sources'][] = [
      'form_id'    => (int) $row->form_id,
      'form_title' => check_plain($row->form_title),
      'q_solr_key' => $solr_key,
      'q_label'    => check_plain($row->q_label),
      'count'      => (int) $row->count,
    ];
  }

  uasort($tags, fn($a, $b) => $b['count'] - $a['count']);

  $totals = array_column($tags, 'count');
  $min    = min($totals);
  $max    = max($totals);
  $range  = ($max - $min) ?: 1;

  $words = [];
  foreach ($tags as $tag) {
    $normalised = ($tag['count'] - $min) / $range;
    $words[] = [
      'text'    => $tag['text'],
      'size'    => (int) round(16 + $normalised * 64),
      'count'   => $tag['count'],
      'sources' => $tag['sources'],
    ];
  }
  return $words;
}

/**
 * Build a Solr field key for a given question datatype and q_id.
 * Single source of truth — used by analytics, filter questions, etc.
 */
function modularform_build_solr_key($datatype, $q_id) {
  $type_map  = array_flip(modularform_type_map());
  module_load_include('inc', 'modularform', 'modularform.rules');
  $solr_map  = solr_field_map();
  $type_name = $type_map[$datatype] ?? NULL;
  $prefix    = $type_name ? ($solr_map[$type_name] ?? NULL) : NULL;
  if (!$prefix) {
    return NULL;
  }
  return $prefix . '_' . $q_id;
}

/**
 * Implements hook_theme() on behalf of modularform_analytics.
 *
 * Declared here; Drupal picks it up because the .inc is loaded via the
 * menu router's 'file' key before theme registry is built on cache clear.
 * Alternatively you can put hook_theme in the .module file if preferred.
 */
function modularform_analytics_theme() {
  return [
    'modularform_analytics_wordcloud' => [
      'template'  => 'modularform-analytics-wordcloud',
      'path'      => drupal_get_path('module', 'modularform_analytics') . '/templates',
      'variables' => [],
    ],
  ];
}
