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

      /* ── State ── */
      var xhr         = null;
      var searchTimer = null;
      var currentPage = 0;
      var initialLoad = true;

      /* ── Bindings ── */
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

      /* ── Core fetch ── */
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
          })
          .fail(function (jqXHR, status) {
            if (status !== 'abort') { showError('request_failed'); }
          })
          .always(function () { setLoading(false); });
      }

      /* ── Render rows ── */
      function renderRows(rows) {
        $empty.prop('hidden', true);
        $tbody.empty();

        if (!rows.length) {
          $empty.prop('hidden', false);
          return;
        }

        var html = '';
        rows.forEach(function (row) {
          var badge = row.status_val
            ? 'mf-badge mf-badge--published'
            : 'mf-badge mf-badge--draft';

          html +=
            '<tr>' +
            '<td class="gform-td-name"><div class="gform-name-inner">' +
            '<span class="gform-form-icon"><svg viewBox="0 0 13 13" fill="white" xmlns="http://www.w3.org/2000/svg">' +
            '<rect x="1.5" y="2" width="10" height="1.8" rx=".9"/>' +
            '<rect x="1.5" y="5.5" width="10" height="1.8" rx=".9"/>' +
            '<rect x="1.5" y="9" width="7" height="1.8" rx=".9"/>' +
            '</svg></span>' +
            '<a href="' + esc(row.urls.edit) + '" class="gform-name-text">' + esc(row.title) + '</a>' +
            '</div></td>' +
            '<td>' + esc(row.owner)   + '</td>' +
            '<td><span class="' + badge + '">' + esc(row.status) + '</span></td>' +
            '<td>' + esc(row.created) + '</td>' +
            '<td class="gform-td-ops">' + buildOps(row.urls) + '</td>' +
            '</tr>';
        });

        $tbody.html(html);
      }

      /* ── Render pager ── */
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
          '<a href="#" data-page="' + (page - 1) + '">' + Drupal.t('« Prev') + '</a></li>';

        pageRange(page, pages).forEach(function (p) {
          if (p === '...') {
            html += '<li class="mf-pager__item mf-pager__ellipsis"><span>…</span></li>';
          } else {
            html += '<li class="mf-pager__item' + (p === page ? ' is-active' : '') + '">' +
              '<a href="#" data-page="' + p + '">' + (p + 1) + '</a></li>';
          }
        });

        html += '<li class="mf-pager__item mf-pager__next' + (page >= pages - 1 ? ' is-disabled' : '') + '">' +
          '<a href="#" data-page="' + (page + 1) + '">' + Drupal.t('Next »') + '</a></li>';

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

      /* ── Page range helper ── */
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

      /* ── Ops dropdown ── */
      function buildOps(urls) {
        return (
          '<div class="action-dropdown">' +
          '<button class="action-dropbtn">' + Drupal.t('Actions') + ' &#9660;</button>' +
          '<div class="action-dropdown-content">' +
          '<a href="' + esc(urls.preview)  + '">' + Drupal.t('Preview Survey')  + '</a>' +
          '<a href="' + esc(urls.edit)     + '">' + Drupal.t('Edit Survey')     + '</a>' +
          '<a href="' + esc(urls.settings) + '">' + Drupal.t('Edit Settings')   + '</a>' +
          '<a href="' + esc(urls.delete)   + '" class="mf-op-delete">' + Drupal.t('Delete Survey') + '</a>' +
          '</div></div>'
        );
      }

      /* ── Meta line ── */
      function updateMeta(total, query) {
        var label = total === 1 ? Drupal.t('form') : Drupal.t('forms');
        var text  = query
          ? total + ' ' + label + ' ' + Drupal.t('matching') + ' <em>' + esc(query) + '</em>'
          : total + ' ' + label;
        $meta.html(text);
      }

      /* ── Helpers ── */
      function setLoading(on) {
        $spinner.toggleClass('mf-spinner--active', on);
        $search.attr('aria-busy', on ? 'true' : 'false');
      }

      function showError(code) {
        var msg = code === 'search_unavailable'
          ? Drupal.t('Search is temporarily unavailable.')
          : Drupal.t('An error occurred. Please refresh and try again.');
        $tbody.html('<tr><td colspan="5" class="gform-error-row">' + msg + '</td></tr>');
      }

      function esc(str) {
        return $('<span>').text(String(str || '')).html();
      }

      /* ── Initial load ── */
      doSearch();

    }
  };

}(jQuery, Drupal));
