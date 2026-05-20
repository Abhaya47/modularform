<?php
/**
 * @file modularform_forms_index.inc
 *
 * Everything search-related for modularform_form entities.
 *
 * INDEX-TIME  (called by Solr when indexing):
 *   modularform_search_form_status_callback()
 *   modularform_search_form_document_callback()
 *   modularform_search_form_reindex_callback()
 *
 */

module_load_include('inc', 'modularform_search', 'includes/modularform_forms_search.solr');

// ===========================================================================
// INDEX-TIME
// ===========================================================================

/**
 * Status callback — only index non-deleted forms.
 * Should I even store this data
 */
function modularform_search_form_status_callback($entity_id, $entity_type, $entity = NULL) {
  if (!empty($entity)) {
    return empty($entity->deleted_at);
  }

  // Fallback: load via entity API
  $entities = entity_load('modularform_form', [$entity_id]);
  $entity = reset($entities);
  return $entity && empty($entity->deleted_at);
}

/**
 * Reindex callback — marks all forms dirty.
 */
function modularform_search_form_reindex_callback($env_id) {
  db_update('apachesolr_index_entities')
    ->condition('entity_type', 'modularform_form')
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
  // Entity was deleted but still in the queue
  if (empty($entity) || empty($entity->title)) {
    return [];
  }

  // full-text searchable
  $document->setField('label', $entity->title);
  if (!empty($entity->description)) {
    $document->setField('ts_description', strip_tags($entity->description));
  }
  $document->setField('ss_slug', $entity->slug);

  // filterable integers
  $document->setField('is_status', (int) $entity->status);
  $document->setField('is_visibility', (int) $entity->visibility);
  $document->setField('is_closes', (int) $entity->closes);
  $document->setField('is_scheduled', (int) $entity->scheduled);
  $document->setField('is_editable', (int) $entity->editable);
  $document->setField('is_anonymous_responses', (int) $entity->anonymous_responses);
  $document->setField('is_created_by', (int) $entity->created_by);

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

  return [$document];
}

