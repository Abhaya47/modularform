/**
 * @file dashboard.js
 * AJAX search + filter for the Modular Form dashboard.
 *
 * Place at: modularform/js/dashboard.js
 *
 * Depends on: jQuery (Drupal core), Tabler icon font (for op icons).
 * Config is injected from PHP via Drupal.settings.modularform:
 *   { ajaxUrl, isAdmin, currentUid, token }
 */
(function ($, Drupal) {
  'use strict';

  Drupal.behaviors.modularformDashboard = {
    attach: function (context, settings) {

      /* Run once per page load */
      var $guard = $('#mf-form-rows', context).once('mf-dashboard');
      if (!$guard.length) { return; }

      /* ── Config from PHP ── */
      var cfg = settings.modularform || {};

      /* ── DOM refs ── */
      var $search     = $('#mf-search');
      var $status     = $('#mf-filter-status');
      var $visibility = $('#mf-filter-visibility');
      var $sort       = $('#mf-filter-sort');
      var $owner      = $('#mf-filter-owner');
      var $tbody      = $('#mf-form-rows');
      var $meta       = $('#mf-meta');
      var $empty      = $('#mf-empty');
      var $spinner    = $('#mf-spinner');

      /* ── State ── */
      var xhr   = null;
      var timer = null;

      /* ────────────────────────────────
         Event bindings
      ──────────────────────────────── */
      $search.on('input', function () {
        clearTimeout(timer);
        timer = setTimeout(doSearch, 300);
      });

      $status.on('change',     doSearch);
      $visibility.on('change', doSearch);
      $sort.on('change',       doSearch);
      $owner.on('change',      doSearch);

      /* ────────────────────────────────
         Core search / filter
      ──────────────────────────────── */
      function doSearch() {
        if (xhr) { xhr.abort(); }

        var params = {
          token      : cfg.token      || '',
          search     : $.trim($search.val()),
          status     : $status.val()     || '',
          visibility : $visibility.val() || '',
          sort       : $sort.val()       || 'created_desc',
          owner_only : $owner.is(':checked') ? '1' : '0',
        };

        setLoading(true);

        xhr = $.ajax({
          url      : cfg.ajaxUrl,
          method   : 'POST',
          data     : params,
          dataType : 'json',
        });

        xhr
          .done(function (data) {
            if (data.error) {
              showError(data.error);
              return;
            }
            renderRows(data.rows || []);
            updateMeta(data.count || 0, params.search);
          })
          .fail(function (jqXHR, status) {
            if (status !== 'abort') {
              showError('request_failed');
            }
          })
          .always(function () {
            setLoading(false);
          });
      }

      /* ────────────────────────────────
         Render table rows from JSON
      ──────────────────────────────── */
      function renderRows(rows) {
        $empty.prop('hidden', true);
        $tbody.empty();

        if (!rows.length) {
          $empty.prop('hidden', false);
          return;
        }

        var html = '';

        rows.forEach(function (row) {
          var badgeClass = row.status_val
            ? 'mf-badge mf-badge--published'
            : 'mf-badge mf-badge--draft';

          html +=
            '<tr>' +
            '<td class="gform-td-name">' +
            '<div class="gform-name-inner">' +
            '<span class="gform-form-icon">' +
            '<svg viewBox="0 0 13 13" fill="white" xmlns="http://www.w3.org/2000/svg">' +
            '<rect x="1.5" y="2"   width="10" height="1.8" rx=".9"/>' +
            '<rect x="1.5" y="5.5" width="10" height="1.8" rx=".9"/>' +
            '<rect x="1.5" y="9"   width="7"  height="1.8" rx=".9"/>' +
            '</svg>' +
            '</span>' +
            '<a href="' + esc(row.urls.edit) + '" class="gform-name-text">' +
            esc(row.title) +
            '</a>' +
            '</div>' +
            '</td>' +
            '<td>' + esc(row.owner)   + '</td>' +
            '<td><span class="' + badgeClass + '">' + esc(row.status) + '</span></td>' +
            '<td>' + esc(row.created) + '</td>' +
            '<td class="gform-td-ops">' + buildOps(row.urls) + '</td>' +
            '</tr>';
        });

        $tbody.html(html);
      }

      /* ── Operations dropdown ── */
      function buildOps(urls) {
        return (
          '<div class="action-dropdown">' +
          '<button class="action-dropbtn">' + Drupal.t('Actions') + ' &#9660;</button>' +
          '<div class="action-dropdown-content">' +
          '<a href="' + esc(urls.preview)  + '">' + Drupal.t('Preview Survey')  + '</a>' +
          '<a href="' + esc(urls.edit)     + '">' + Drupal.t('Edit Survey')     + '</a>' +
          '<a href="' + esc(urls.settings) + '">' + Drupal.t('Edit Settings')   + '</a>' +
          '<a href="' + esc(urls.delete)   + '" class="mf-op-delete">' + Drupal.t('Delete Survey') + '</a>' +
          '</div>' +
          '</div>'
        );
      }

      /* ────────────────────────────────
         Meta line  e.g. "12 forms" / "3 forms matching survey"
      ──────────────────────────────── */
      function updateMeta(count, query) {
        var label = count === 1 ? Drupal.t('form') : Drupal.t('forms');
        var text  = query
          ? count + ' ' + label + ' ' + Drupal.t('matching') + ' <em>' + esc(query) + '</em>'
          : count + ' ' + label;
        $meta.html(text);
      }

      /* ────────────────────────────────
         Loading + error helpers
      ──────────────────────────────── */
      function setLoading(on) {
        $spinner.toggleClass('mf-spinner--active', on);
        $search.attr('aria-busy', on ? 'true' : 'false');
      }

      function showError(code) {
        var msg = (code === 'search_unavailable')
          ? Drupal.t('Search is temporarily unavailable. Please try again.')
          : Drupal.t('An error occurred. Please refresh and try again.');

        $tbody.html(
          '<tr><td colspan="5" class="gform-error-row">' + msg + '</td></tr>'
        );
      }

      /* ────────────────────────────────
         XSS-safe HTML escape
      ──────────────────────────────── */
      function esc(str) {
        return $('<span>').text(String(str || '')).html();
      }

    } /* attach */
  }; /* behavior */

}(jQuery, Drupal));
