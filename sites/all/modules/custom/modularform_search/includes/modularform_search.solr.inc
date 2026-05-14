<?php
/**
 * @file modularform_search.solr.inc
 *
 * Raw Solr engine — shared by all entity .inc files.
 *
 * PRIVATE. Other modules must NOT call these functions directly.
 * Call the public API in .forms.inc / .responses.inc instead.
 *
 * Functions:
 *   _modularform_search_solr_query()   — execute a Solr search
 *   _modularform_search_sql_fallback() — LIKE-based fallback when Solr is down
 */

/**
 * Execute a Solr query and return raw Solr docs.
 *
 * @param string $search
 *   The user's search string.
 * @param array $params
 *   Solr params: qf, fq, fl, rows, defType, etc.
 *   Callers build these — this function just fires the query.
 *
 * @return array
 *   Array of Solr doc objects, or empty array on no results.
 *
 * @throws Exception
 *   Re-throws if Solr is unreachable — caller handles fallback.
 */
function _modularform_search_solr_query($search, array $params) {
  try {
    $env_id = apachesolr_default_environment();
    $solr   = apachesolr_get_solr($env_id);
    $solr->ping();  // ← if this throws, Solr is down
    $response = $solr->search($search, $params);
    return $response->response->docs ?: [];
  }
  catch (Exception $e) {
    watchdog('modularform_search', 'Solr is down: @msg', ['@msg' => $e->getMessage()], WATCHDOG_ERROR);
    return NULL;  // ← means "Solr down", not "no results"
  }
}
