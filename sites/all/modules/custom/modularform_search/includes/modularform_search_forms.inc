<?php
/**
 * @file modularform_search_forms.inc
 *
 * Everything search-related for modularform_form entities.
 *
 * INDEX-TIME  (called by Solr when indexing):
 *   modularform_search_form_status_callback()
 *   modularform_search_form_document_callback()
 *   modularform_search_form_reindex_callback()
 *
 */

module_load_include('inc', 'modularform_search', 'includes/modularform_search.solr');

// ===========================================================================
// INDEX-TIME
// ===========================================================================

/**
 * Status callback — only index non-deleted forms.
 * Should I even store this data
 */
function modularform_search_form_status_callback($entity, $entity_type) {
  return empty($entity->deleted_at);  // TRUE = index, FALSE = skip
}

/**
 * Reindex callback — marks all forms dirty.
 */
function modularform_search_form_reindex_callback($env_id) {
  db_update('apachesolr_index_entities_modularform_form')
    ->fields(['changed' => REQUEST_TIME, 'status' => 1])
    ->execute();
}



/**
 * Document enrichment — called from hook_apachesolr_index_documents_alter().
 *
 * Adds stored fields the search queries rely on:
 *   bs_deleted       — soft-delete flag for filtering
 *   is_status        — form status (active/inactive)
 *   is_visibility    — visibility setting
 *   ts_form_description — full-text description
 */
function modularform_search_form_document_callback($document, $entity, $entity_type, $env_id) {
  // full-text searchable
  $document->setField('label', $entity->title);
  $document->setField('ts_description', strip_tags($entity->description));
  $document->setField('ss_slug',        $entity->slug);

  // filterable integers
  $document->setField('is_status',             (int) $entity->status);
  $document->setField('is_visibility',         (int) $entity->visibility);
  $document->setField('is_closes',             (int) $entity->closes);
  $document->setField('is_scheduled',          (int) $entity->scheduled);
  $document->setField('is_editable',           (int) $entity->editable);
  $document->setField('is_anonymous_responses',(int) $entity->anonymous_responses);
  $document->setField('is_created_by',         (int) $entity->created_by);

  // filterable dates (stored as timestamps)
  // Only set date fields if they have a real value
  if (!empty($entity->closes_at)) {
    $document->setField('ds_closes_at', gmdate('Y-m-d\TH:i:s\Z', $entity->closes_at));
  }
  if (!empty($entity->scheduled_date)) {
    $document->setField('ds_scheduled_date', gmdate('Y-m-d\TH:i:s\Z', $entity->scheduled_date));
  }

  // bs_ fields must be explicit true/false, never empty
  $document->setField('bs_deleted', (bool) !empty($entity->deleted_at));
  $document->setField('ds_created_at', gmdate('Y-m-d\TH:i:s\Z', $entity->created_at));
  $document->setField('ds_updated_at', gmdate('Y-m-d\TH:i:s\Z', $entity->updated_at));

  // soft delete
  $document->setField('bs_deleted', !empty($entity->deleted_at));
  return [$document];
}

// ===========================================================================
// SEARCH-TIME (public API)
// ===========================================================================

/**
 * Simple title search — for a plain search box.
 *
 * Usage:
 *   module_load_include('inc', 'modularform_search',
 * 'includes/modularform_search.forms');
 *   $forms = modularform_search_forms_by_title($search_string);
 *
 * @param string $search
 *
 * @return array stdClass rows with: id, title, status, created_by, created_at
 */
function modularform_search_forms_by_title($search) {
  return modularform_search_query_forms(['title' => $search]);
}

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
  $search = isset($conditions['title']) ? $conditions['title'] : '';

  // Build filter queries from conditions (these narrow Solr results).
  $fq = ['bundle:modularform_form', 'bs_deleted:false'];

  if (isset($conditions['status'])) {
    $fq[] = 'is_status:' . (int) $conditions['status'];
  }
  if (isset($conditions['visibility'])) {
    $fq[] = 'is_visibility:' . (int) $conditions['visibility'];
  }
  if (isset($conditions['created_by'])) {
    $fq[] = 'is_uid:' . (int) $conditions['created_by'];
  }

  $params = [
    'defType' => 'edismax', //parser
    'qf' => 'label^3.0 ts_description^1.0', //answer hierarchy so title ranks above description match
    'fq' => 'bundle:modularform_form',
    'fl' => 'entity_id',
    'rows' => 50,
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
