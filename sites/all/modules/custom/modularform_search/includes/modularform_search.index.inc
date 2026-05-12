<?php
/**
 * @file modularform_search.index.inc
 *
 * Enriches Solr documents for modularform entities before they are
 * sent to the Solr index.
 *
 * Apache Solr Search calls hook_apachesolr_index_documents_alter() which
 * routes here. We add custom stored fields so deep search can:
 *   1. Group results back to a parent form  (ss_form_id)
 *   2. Search richer text fields            (ts_section_name, ts_question_label, etc.)
 *   3. Filter out soft-deleted records      (bs_deleted)
 *
 * Field-naming conventions follow apache_solr_search dynamic field types:
 *   ss_* = stored string (single value, not tokenised)
 *   ts_* = stored text (tokenised, full-text searchable)
 *   bs_* = stored boolean
 *   is_* = stored integer
 */

/**
 * Enrich Solr documents for modularform entity types.
 *
 * @param array $documents
 *   Array of ApacheSolrDocument objects passed by reference.
 * @param object $entity
 *   The entity being indexed.
 * @param string $entity_type
 *   One of: modularform_form, modularform_section, modularform_question.
 */
function modularform_search_index_alter_documents(array &$documents, $entity, $entity_type) {
  foreach ($documents as $document) {
    // Tag every document with its entity type for easy filtering.
    $document->setField('ss_entity_type', $entity_type);

    switch ($entity_type) {

      // ── Form ──────────────────────────────────────────────────────────────
      case 'modularform_form':
        // Soft-delete flag — lets us filter deleted forms out of results.
        $document->setField('bs_deleted', !empty($entity->deleted_at));

        // Status and visibility stored for potential future faceting.
        $document->setField('is_status',     (int) $entity->status);
        $document->setField('is_visibility', (int) $entity->visibility);

        // Description as searchable text (apache_solr may not index 'text' fields
        // by default — we make sure it lands in a ts_ field too).
        if (!empty($entity->description)) {
          $document->setField('ts_form_description', strip_tags($entity->description));
        }
        break;

      // ── Section ───────────────────────────────────────────────────────────
      case 'modularform_section':
        // Critical: store parent form_id so we can group results.
        $document->setField('ss_form_id', (string) $entity->form_id);

        // Soft-delete inherited from parent form — sections have no deleted_at,
        // so we check the parent form.
        $parent_deleted = db_select('modularform_forms', 'f')
          ->fields('f', ['deleted_at'])
          ->condition('f.id', $entity->form_id)
          ->execute()
          ->fetchField();
        $document->setField('bs_deleted', !empty($parent_deleted));

        // Section name as searchable text.
        if (!empty($entity->name)) {
          $document->setField('ts_section_name', $entity->name);
          // Also set the generic title field so the default search field works.
          $document->setField('title', $entity->name);
        }
        break;

      // ── Question ──────────────────────────────────────────────────────────
      case 'modularform_question':
        // Critical: store parent form_id for grouping.
        $document->setField('ss_form_id', (string) $entity->form_id);

        // Soft-delete inherited from parent form.
        $parent_deleted = db_select('modularform_forms', 'f')
          ->fields('f', ['deleted_at'])
          ->condition('f.id', $entity->form_id)
          ->execute()
          ->fetchField();
        $document->setField('bs_deleted', !empty($parent_deleted));

        // Question label — highest value for search.
        if (!empty($entity->label)) {
          $document->setField('ts_question_label', $entity->label);
          $document->setField('title', $entity->label);
        }

        // Question description.
        if (!empty($entity->description)) {
          $document->setField('ts_question_description', strip_tags($entity->description));
        }

        // Placeholder text — useful for "enter your email"-style searches.
        if (!empty($entity->placeholder)) {
          $document->setField('ts_question_placeholder', $entity->placeholder);
        }

        // Question type integer — stored for potential future filtering.
        $document->setField('is_question_type', (int) $entity->type);
        break;
    }
  }
}

/**
 * Re-index all modularform entities.
 *
 * Call this from drush or an admin page after enabling the module to
 * ensure all existing records are in the Solr index.
 *
 * Usage (Drush):
 *   drush php-eval "module_load_include('inc', 'modularform_search',
 *     'includes/modularform_search.index');
 *     modularform_search_reindex_all();"
 */
function modularform_search_reindex_all() {
  if (!module_exists('apachesolr')) {
    watchdog('modularform_search', 'apachesolr module not available — cannot re-index.', [], WATCHDOG_ERROR);
    return;
  }

  $env_id = apachesolr_default_environment();

  // Mark all three entity types for re-indexing.
  $entity_types = ['modularform_form', 'modularform_section', 'modularform_question'];
  foreach ($entity_types as $entity_type) {
    apachesolr_mark_entity_dirty($env_id, $entity_type);
  }

  watchdog('modularform_search', 'All modularform entities marked for re-indexing.', [], WATCHDOG_INFO);
}
