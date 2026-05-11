(function ($, Drupal) {
  Drupal.behaviors.modularformPeople = {
    attach: function (context, settings) {
      $('#modularform-people-select', context).once('modularform-people').each(function () {
        var $select = $(this);
        var $hidden = $('#modularform-people-input');
        var acUrl   = $select.data('ac-url');

        $select.select2({
          tags: true,
          multiple: true,
          placeholder: Drupal.t('Search by name or email, or type an email to invite\u2026'),
          minimumInputLength: 1,

          initSelection: function (element, callback) {
            var val = $hidden.val();
            if (!val) { callback([]); return; }
            var items = [];
            $.each(val.split(','), function (i, part) {
              part = $.trim(part);
              if (!part) { return; }
              if (part.indexOf('email:') === 0) {
                var addr = part.slice(6);
                items.push({ id: part, text: addr + ' (' + Drupal.t('invite') + ')' });
              }
              else if (part.indexOf('uid:') === 0) {
                items.push({ id: part, text: part });
              }
            });
            callback(items);
          },

          query: function (opts) {
            $.ajax({
              url: acUrl,
              dataType: 'json',
              data: { people: opts.term },
              success: function (data) {
                opts.callback({ results: data.results, more: false });
              },
              error: function () {
                opts.callback({ results: [] });
              }
            });
          },

          createSearchChoice: function (term, data) {
            var emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
            var match = data.filter(function (item) {
              return item.text.toLowerCase() === term.toLowerCase();
            });
            if (match.length === 0 && emailRegex.test(term)) {
              return { id: 'email:' + term, text: term + ' (' + Drupal.t('invite') + ')', invite: true };
            }
          },

          formatSelection: function (item) {
            return item.text.replace(/\s*\(invite\)\s*$/, '');
          },

          formatResult: function (item) {
            if (item.invite) {
              return '<span class="select2-result-invite">' + item.text + '</span>';
            }
            return '<span class="select2-result-user">' + item.text + '</span>';
          }
        });

        // Sync into the hidden field on every change
        $select.on('change', function () {
          var val = $select.select2('val');
          $hidden.val(val ? val.join(',') : '');
          $hidden.trigger('change');
        });
      });
    }
  };
}(jQuery, Drupal));
