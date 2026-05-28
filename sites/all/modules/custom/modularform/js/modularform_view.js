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
      var filterOverlay = $('#mfv-filterable-form-wrapper');

      function openOverlay() {
        filterOverlay.css('display', '');
        filterBtn.attr('aria-expanded', 'true');
      }

      function closeOverlay() {
        filterOverlay.css('display', 'none');
        filterBtn.attr('aria-expanded', 'false');
      }

      if (filterBtn.length) {
        filterBtn.on('click', function () {
          filterOverlay.css('display') === 'none' ? openOverlay() : closeOverlay();
        });
      }



    }
  };

}(jQuery, Drupal));
