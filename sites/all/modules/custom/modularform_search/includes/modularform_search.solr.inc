<?php
/**
 * @file modularform_search.solr.inc
 * Solr title search for modularform forms.
 */

function modularform_search_solr_title($search) {
  try {
    $env_id = apachesolr_default_environment();
    $solr   = apachesolr_get_solr($env_id);
    $solr->ping();

    $params = [
      'defType' => 'edismax',
      'qf'      => 'title^3.0 ts_form_description^1.0', //answer hierarchy so title ranks above description match
      'fq'      => 'bundle:modularform_form',
      'fl'      => 'entity_id,title',
      'rows'    => 50,
    ];

    $response = $solr->search($search, $params);

    if (empty($response->response->docs)) {
      return [];
    }

    // Get matching IDs from Solr
    $ids = [];
    foreach ($response->response->docs as $doc) {
      $ids[] = (int) $doc->entity_id;
    }

    // Load full rows from DB so we have all fields the dashboard needs
    return db_select('modularform_forms', 'f')
      ->fields('f', ['id', 'title', 'status', 'created_by', 'created_at'])
      ->condition('f.id', $ids, 'IN')
      ->isNull('f.deleted_at')
      ->orderBy('f.created_at', 'DESC')
      ->execute()
      ->fetchAll();
  }
  catch (Exception $e) {
    // Solr down — fall back to SQL
    watchdog('modularform_search', 'Solr unavailable: @msg', ['@msg' => $e->getMessage()], WATCHDOG_WARNING);
    return _modularform_search_sql_fallback($search);
  }
}

function _modularform_search_sql_fallback($search) {
  return db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title', 'created_by', 'created_at'])
    ->isNull('f.deleted_at')
    ->condition('f.title', '%' . db_like($search) . '%', 'LIKE')
    ->orderBy('f.created_at', 'DESC')
    ->execute()
    ->fetchAll();
}
