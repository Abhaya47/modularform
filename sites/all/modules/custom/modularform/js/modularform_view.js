(function ($, Drupal) {
  'use strict';

  Drupal.behaviors.modularformView = {
    attach: function (context, settings) {

      // ── Actions dropdown ─────────────────────────────────────────────────
      var toggle = $('#mfv-actions-toggle', context).once('mfv-dropdown');
      var menu   = $('#mfv-actions-menu');

      if (toggle.length) {
        toggle.on('click', function (e) {
          e.stopPropagation();
          var open = !menu.prop('hidden');
          menu.prop('hidden', open);
          toggle.attr('aria-expanded', String(!open));
        });

        $(document).on('click.mfvDropdown', function () {
          menu.prop('hidden', true);
          toggle.attr('aria-expanded', 'false');
        });

        menu.on('click', function (e) { e.stopPropagation(); });
      }

      // ── Filterable overlay toggle ────────────────────────────────────────
      var filterBtn     = $('#mfv-filter-toggle', context).once('mfv-filter');
      var filterOverlay = $('#mfv-filter-overlay');

      function openOverlay() {
        filterOverlay.prop('hidden', false);
        filterBtn.attr('aria-expanded', 'true');
        var $search = $('#mfv-overlay-search');
        if ($search.length) {
          $search.val('');
          runSearch('');
          $search.trigger('focus');
        }
      }

      function closeOverlay() {
        filterOverlay.prop('hidden', true);
        filterBtn.attr('aria-expanded', 'false');
      }

      if (filterBtn.length) {
        filterBtn.on('click', function () {
          filterOverlay.prop('hidden') ? openOverlay() : closeOverlay();
        });
      }

      // Cancel button — re-bound after every AJAX rebuild via attachBehaviors.
      $(context).on('click', '#mfv-filter-cancel', function (e) {
        e.preventDefault();
        closeOverlay();
      });

      // ── Live search inside overlay ───────────────────────────────────────
      function runSearch(query) {
        var q = query.toLowerCase().trim();
        $('#mfv-overlay-questions .mfv-overlay-q-row', context).each(function () {
          var label = ($(this).data('label') || $(this).text()).toLowerCase();
          $(this).toggle(!q || label.indexOf(q) !== -1);
        });
      }

      $(context).on('input', '#mfv-overlay-search', function () {
        runSearch($(this).val());
      });

    }
  };

}(jQuery, Drupal));
