/**
 * @file dashboard.js
 */
(function ($, Drupal) {
  'use strict';

  Drupal.behaviors.modularformDashboard = {
    attach: function (context, settings) {

      var $guard = $('#mf-form-rows', context).once('mf-dashboard');
      if (!$guard.length) { return; }

      var cfg   = settings.modularform || {};
      var LIMIT = cfg.perPage || 25;

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
      var $pager      = $('#mf-pager');
      var $table      = $('#mf-table');

      /* ── Export selection DOM refs ── */
      var $exportToggle = $('#mf-export-toggle');
      var $exportBar    = $('#mf-export-bar');
      var $exportCount  = $('#mf-export-bar-count');
      var $exportGo     = $('#mf-export-bar-go');
      var $exportCancel = $('#mf-export-bar-cancel');
      var $checkAll     = $('#mf-check-all');

      /* ── State ── */
      var xhr         = null;
      var searchTimer = null;
      var currentPage = 0;
      var initialLoad = true;
      var selecting   = false;

      /* ══════════════════════════════════════════
         EXPORT SELECTION MODE
      ══════════════════════════════════════════ */

      function enterSelectMode() {
        selecting = true;
        $table.addClass('is-selecting');
        $exportToggle.addClass('is-active').attr('aria-pressed', 'true');
        $table.find('.mf-row-check, #mf-check-all').attr('tabindex', '0');
        updateExportBar();
      }

      function exitSelectMode() {
        selecting = false;
        $table.removeClass('is-selecting');
        $exportToggle.removeClass('is-active').attr('aria-pressed', 'false');
        $checkAll.prop('checked', false);
        $tbody.find('.mf-row-check').prop('checked', false);
        $table.find('.mf-row-check, #mf-check-all').attr('tabindex', '-1');
        $exportBar.removeClass('is-visible');
      }

      $exportToggle.on('click', function () {
        if (selecting) { exitSelectMode(); } else { enterSelectMode(); }
      });

      $exportCancel.on('click', function () {
        exitSelectMode();
      });

      $checkAll.on('change', function () {
        var checked = $(this).is(':checked');
        $tbody.find('.mf-row-check').prop('checked', checked);
        updateExportBar();
      });

      $tbody.on('change', '.mf-row-check', function () {
        var total   = $tbody.find('.mf-row-check').length;
        var checked = $tbody.find('.mf-row-check:checked').length;
        $checkAll.prop('indeterminate', checked > 0 && checked < total);
        $checkAll.prop('checked', checked === total && total > 0);
        updateExportBar();
      });

      $tbody.on('click', 'tr', function (e) {
        if (!selecting) { return; }
        if ($(e.target).is('input, a, button') || $(e.target).closest('a, button, .action-dropdown').length) {
          return;
        }
        var $cb = $(this).find('.mf-row-check');
        $cb.prop('checked', !$cb.is(':checked')).trigger('change');
      });

      function getCheckedFids() {
        return $tbody.find('.mf-row-check:checked').map(function () {
          return $(this).val();
        }).get();
      }

      function updateExportBar() {
        var fids = getCheckedFids();
        if (fids.length === 0) {
          $exportBar.removeClass('is-visible');
          return;
        }
        var label = fids.length === 1
          ? Drupal.t('1 form selected')
          : Drupal.t('@n forms selected', {'@n': fids.length});
        $exportCount.text(label);
        $exportGo.attr('href', Drupal.settings.basePath + 'modularform/export/page?fids=' + fids.join(','));
        $exportBar.addClass('is-visible');
      }

      /* ══════════════════════════════════════════
         SEARCH & FILTER BINDINGS
      ══════════════════════════════════════════ */

      $search.on('input', function () {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(function () {
          currentPage = 0;
          doSearch();
        }, 300);
      });

      $status.on('change',     function () { currentPage = 0; doSearch(); });
      $visibility.on('change', function () { currentPage = 0; doSearch(); });
      $sort.on('change',       function () { currentPage = 0; doSearch(); });
      $owner.on('change',      function () { currentPage = 0; doSearch(); });

      /* ══════════════════════════════════════════
         CORE FETCH
      ══════════════════════════════════════════ */

      function doSearch() {
        if (xhr) { xhr.abort(); }

        setLoading(true);

        xhr = $.ajax({
          url:      cfg.ajaxUrl,
          method:   'POST',
          dataType: 'json',
          data: {
            token:      cfg.token || '',
            search:     $.trim($search.val()),
            status:     $status.val()     || '',
            visibility: $visibility.val() || '',
            sort:       $sort.val()       || 'created_desc',
            owner_only: $owner.is(':checked') ? '1' : '0',
            page:       currentPage,
          },
        });

        xhr
          .done(function (data) {
            if (data.error) { showError(data.error); return; }
            if (!initialLoad) { renderRows(data.rows || []); }
            initialLoad = false;
            renderPager(data.total || 0, data.page || 0);
            updateMeta(data.total || 0, $.trim($search.val()));
            if (selecting) {
              $tbody.find('.mf-row-check').attr('tabindex', '0');
              updateExportBar();
            }
          })
          .fail(function (jqXHR, status) {
            if (status !== 'abort') { showError('request_failed'); }
          })
          .always(function () { setLoading(false); });
      }

      /* ══════════════════════════════════════════
         RENDER ROWS
      ══════════════════════════════════════════ */

      function renderRows(rows) {
        $empty.prop('hidden', true);
        $tbody.empty();

        if (!rows.length) {
          $empty.prop('hidden', false);
          return;
        }

        var cbTabindex = selecting ? '0' : '-1';
        var html = '';

        rows.forEach(function (row) {
          var urls = row.urls || {};

          var badge = row.status_val
            ? 'mf-badge mf-badge--published'
            : 'mf-badge mf-badge--draft';

          var respCount     = parseInt(row.response_count || 0, 10);
          var respPillClass = 'mf-resp-pill' + (respCount === 0 ? ' mf-resp-pill--empty' : '');
          var respLabel     = respCount === 1 ? Drupal.t('response') : Drupal.t('responses');

          // pill inner HTML shared between link and span
          var respInner =
            '<svg class="mf-resp-pill__icon" viewBox="0 0 14 14" fill="none" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">' +
            '<path d="M7 1.5C3.96 1.5 1.5 3.69 1.5 6.4c0 1.18.46 2.26 1.22 3.1L2 12.5l2.9-1.1A6.1 6.1 0 0 0 7 11.3c3.04 0 5.5-2.19 5.5-4.9S10.04 1.5 7 1.5Z" fill="currentColor" opacity=".18"/>' +
            '<path d="M7 1.5C3.96 1.5 1.5 3.69 1.5 6.4c0 1.18.46 2.26 1.22 3.1L2 12.5l2.9-1.1A6.1 6.1 0 0 0 7 11.3c3.04 0 5.5-2.19 5.5-4.9S10.04 1.5 7 1.5Z" stroke="currentColor" stroke-width="1.1" stroke-linejoin="round"/>' +
            '<rect x="4.5" y="5.5" width="5" height="1" rx=".5" fill="currentColor"/>' +
            '<rect x="4.5" y="7.5" width="3" height="1" rx=".5" fill="currentColor"/>' +
            '</svg>' +
            '<span class="mf-resp-pill__count">' + respCount + '</span>' +
            '<span class="mf-resp-pill__label">' + respLabel + '</span>';

          // name link — view takes priority, fall back to fill/answer
          var nameUrl  = urls.view || urls.answer || null;
          var nameCell = nameUrl
            ? '<a href="' + esc(nameUrl) + '" class="gform-name-text">' + esc(row.title) + '</a>'
            : '<span class="gform-name-text">' + esc(row.title) + '</span>';

          // responses pill — only a link if the user can view responses
          var respCell = urls.responses
            ? '<a href="' + esc(urls.responses) + '" class="' + respPillClass + '" title="' + Drupal.t('View all responses') + '">' + respInner + '</a>'
            : '<span class="' + respPillClass + '">' + respInner + '</span>';

          html +=
            '<tr data-fid="' + esc(row.id) + '">' +

            /* Checkbox */
            '<td class="gform-td-check" aria-hidden="true">' +
            '<input type="checkbox" class="mf-row-check" value="' + esc(row.id) + '"' +
            ' aria-label="' + Drupal.t('Select') + ' ' + esc(row.title) + '"' +
            ' tabindex="' + cbTabindex + '"/>' +
            '</td>' +

            /* Name */
            '<td class="gform-td-name"><div class="gform-name-inner">' +
            '<span class="gform-form-icon">' +
            '<svg viewBox="0 0 13 13" fill="white" xmlns="http://www.w3.org/2000/svg">' +
            '<rect x="1.5" y="2" width="10" height="1.8" rx=".9"/>' +
            '<rect x="1.5" y="5.5" width="10" height="1.8" rx=".9"/>' +
            '<rect x="1.5" y="9" width="7" height="1.8" rx=".9"/>' +
            '</svg></span>' +
            nameCell +
            '</div></td>' +

            /* Responses */
            '<td class="gform-td-responses">' + respCell + '</td>' +

            /* Owner */
            '<td>' + esc(row.owner) + '</td>' +

            /* Status */
            '<td><span class="' + badge + '">' + esc(row.status) + '</span></td>' +

            /* Created */
            '<td>' + esc(row.created) + '</td>' +

            /* Ops */
            '<td class="gform-td-ops">' + buildOps(urls) + '</td>' +

            '</tr>';
        });

        $tbody.html(html);
      }

      /* ══════════════════════════════════════════
         RENDER PAGER
      ══════════════════════════════════════════ */

      function renderPager(total, page) {
        var pages = Math.ceil(total / LIMIT);

        $pager.empty();

        if (pages <= 1) {
          $pager.hide();
          return;
        }

        $pager.show();

        var html = '<ul class="mf-pager">';

        html += '<li class="mf-pager__item mf-pager__prev' + (page === 0 ? ' is-disabled' : '') + '">' +
          '<a href="#" data-page="' + (page - 1) + '">' + Drupal.t('← Prev') + '</a></li>';

        pageRange(page, pages).forEach(function (p) {
          if (p === '...') {
            html += '<li class="mf-pager__item mf-pager__ellipsis"><span>…</span></li>';
          } else {
            html += '<li class="mf-pager__item' + (p === page ? ' is-active' : '') + '">' +
              '<a href="#" data-page="' + p + '">' + (p + 1) + '</a></li>';
          }
        });

        html += '<li class="mf-pager__item mf-pager__next' + (page >= pages - 1 ? ' is-disabled' : '') + '">' +
          '<a href="#" data-page="' + (page + 1) + '">' + Drupal.t('Next →') + '</a></li>';

        html += '</ul>';
        $pager.html(html);

        $pager.find('a[data-page]').on('click', function (e) {
          e.preventDefault();
          var p = parseInt($(this).data('page'), 10);
          if (p < 0 || p >= pages) { return; }
          currentPage = p;
          doSearch();
          $('html, body').animate({ scrollTop: $('#mf-table-wrap').offset().top - 20 }, 200);
        });
      }

      /* ══════════════════════════════════════════
         HELPERS
      ══════════════════════════════════════════ */

      function pageRange(current, total) {
        if (total <= 7) {
          var r = [];
          for (var i = 0; i < total; i++) { r.push(i); }
          return r;
        }
        var pages = [0];
        if (current > 2) { pages.push('...'); }
        for (var p = Math.max(1, current - 1); p <= Math.min(total - 2, current + 1); p++) {
          pages.push(p);
        }
        if (current < total - 3) { pages.push('...'); }
        pages.push(total - 1);
        return pages;
      }

      function buildOps(urls) {
        var items = '';

        if (urls.preview) {
          items += '<a href="' + esc(urls.preview) + '">' + Drupal.t('Preview Survey') + '</a>';
        }
        if (urls.edit) {
          items += '<a href="' + esc(urls.edit) + '">' + Drupal.t('Edit Survey') + '</a>';
        }
        if (urls.settings) {
          items += '<a href="' + esc(urls.settings) + '">' + Drupal.t('Edit Settings') + '</a>';
        }
        if (urls.answer) {
          items += '<a href="' + esc(urls.answer) + '">' + Drupal.t('Fill Form') + '</a>';
        }
        if (urls.delete) {
          items += '<a href="' + esc(urls.delete) + '" class="mf-op-delete">' + Drupal.t('Delete Survey') + '</a>';
        }

        if (!items) {
          return '<span class="mf-no-ops">' + Drupal.t('—') + '</span>';
        }

        return (
          '<div class="action-dropdown">' +
          '<button class="action-dropbtn">' + Drupal.t('Actions') + ' &#9660;</button>' +
          '<div class="action-dropdown-content">' + items + '</div>' +
          '</div>'
        );
      }

      function updateMeta(total, query) {
        var label = total === 1 ? Drupal.t('form') : Drupal.t('forms');
        var text  = query
          ? total + ' ' + label + ' ' + Drupal.t('matching') + ' <em>' + esc(query) + '</em>'
          : total + ' ' + label;
        $meta.html(text);
      }

      function setLoading(on) {
        $spinner.toggleClass('mf-spinner--active', on);
        $search.attr('aria-busy', on ? 'true' : 'false');
      }

      function showError(code) {
        var msg = code === 'search_unavailable'
          ? Drupal.t('Search is temporarily unavailable.')
          : Drupal.t('An error occurred. Please refresh and try again.');
        $tbody.html('<tr><td colspan="7" class="gform-error-row">' + msg + '</td></tr>');
      }

      function esc(str) {
        return $('<span>').text(String(str || '')).html();
      }

      /* ── Initial load ── */
      doSearch();

    }
  };

}(jQuery, Drupal));
