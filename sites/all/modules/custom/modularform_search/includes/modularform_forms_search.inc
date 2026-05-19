<?php
/**
 * @file modularform_forms_search.inc
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
 * Filtered form search — for search box + dropdowns combined.
 *
 * Usage:
 *   $forms = modularform_search_query_forms([
 *     'title'      => 'survey',   // text search
 *     'status'     => 1,          // filter by status
 *     'visibility' => 0,          // filter by visibility
 *     'created_by' => $uid,       // filter by author
 *   ]);
 *
 * @param array $conditions
 *   Supported keys: title, status, visibility, created_by.
 *
 * @return array stdClass rows with: id, title, status, created_by, created_at
 */
function modularform_search_query_forms(array $conditions = []) {
  $search = isset($conditions['search']) ? $conditions['search'] : '';

  // Build filter queries from conditions (these narrow Solr results).
  $fq = ['bundle:modularform_form', 'bs_deleted:false'];

  if (isset($conditions['status'])) {
    $fq[] = 'is_status:' . (int) $conditions['status'];
  }
  if (isset($conditions['visibility'])) {
    $fq[] = 'is_visibility:' . (int) $conditions['visibility'];
  }
  if (isset($conditions['created_by'])) {
    $fq[] = 'is_created_by:' . (int) $conditions['created_by'];  }

  $params = [
    'defType' => 'edismax',
    'qf'      => 'label^3.0 ts_description^1.0',
    'fq'      => $fq,   // <-- use the array you built above
    'fl'      => 'entity_id',
    'rows'    => 50,
  ];

  $docs = _modularform_search_solr_query($search, $params);

  if ($docs === NULL) {
    // Solr is down
    return ['error' => 'search_unavailable'];
  }

  if (empty($docs)) {
    // Solr is fine, just no matches — caller uses theme('apachesolr_search_noresults')
    return [];
  }

  $ids = array_map(function($doc) {
    return (int) $doc->entity_id;
  }, $docs);

  //  double checking if something slips through solr
  $query = db_select('modularform_forms', 'f')
    ->fields('f', ['id', 'title', 'status', 'created_by', 'created_at'])
    ->condition('f.id', $ids, 'IN')
    ->isNull('f.deleted_at')
    ->orderBy('f.created_at', 'DESC');

  // Mirror the same filters so stale Solr hits don't leak through
  if (isset($conditions['status'])) {
    $query->condition('f.status', (int) $conditions['status']);
  }
  if (isset($conditions['visibility'])) {
    $query->condition('f.visibility', (int) $conditions['visibility']);
  }
  if (isset($conditions['created_by'])) {
    $query->condition('f.created_by', (int) $conditions['created_by']);
  }

  return $query->execute()->fetchAll();
}

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
