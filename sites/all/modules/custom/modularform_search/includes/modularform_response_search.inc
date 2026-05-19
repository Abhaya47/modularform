<?php

module_load_include('inc', 'modularform', 'includes/response_list');
/**
 * Fetch distinct answer values for a given Solr field via faceting.
 *
 * @param string $solr_key
 *   The Solr field name, e.g. ss_val_q_6_6a05bb09ddc79.
 * @param int $form_id
 *   The form ID — used as a filter to scope facets to one form's responses.
 * @param string $term
 *   Optional search term to filter returned values client-side.
 *
 * @return array|NULL
 *   Array of distinct value strings, empty array if none, NULL if Solr is down.
 */
function _modularform_filter_get_answer_values($solr_key, $form_id, $term = '') {
  try {
    $env_id = apachesolr_default_environment();
    $solr   = apachesolr_get_solr($env_id);
    $solr->ping();

    $params = [
      'fq' => [
        'is_form_id:' . (int) $form_id,
        // additional fq entries added here get ANDed automatically by Solr
      ],
      'facet'         => 'true',
      'facet.field'   => $solr_key,
      'facet.limit'   => -1,      // all values
      'facet.mincount'=> 1,       // only values that actually exist
      'facet.sort'    => 'index',  // alphabetical
      'rows'          => 0,        // docs not needed, facets only
      'fl'            => 'id',     // minimal field list since rows=0
    ];

    $response = $solr->search('*:*', $params);

    // Facet counts come back as { value: count, value: count, … }
    $facets = $response->facet_counts->facet_fields->{$solr_key};
    if (empty($facets)) {
      return [];
    }

    $values = array_keys((array) $facets);

    // Optional client-side term filter — Solr facet.prefix could do this
    // server-side too, but the value lists are typically small enough.
    if ($term) {
      $lower = drupal_strtolower($term);
      $values = array_filter($values, function ($v) use ($lower) {
        return strpos(drupal_strtolower($v), $lower) !== FALSE;
      });
    }

    return array_values($values);
  }
  catch (Exception $e) {
    watchdog('modularform', 'Solr facet query failed for field @key: @msg', [
      '@key' => $solr_key,
      '@msg' => $e->getMessage(),
    ], WATCHDOG_ERROR);
    return NULL; // NULL = Solr down, caller handles fallback
  }
}
