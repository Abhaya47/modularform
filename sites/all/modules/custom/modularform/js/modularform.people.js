(function ($) {
  Drupal.behaviors.modularformPeople = {
    attach: function (context, settings) {

      // ── Existing people list — delete on click ──────────────────────────
      $('#modularform-people-existing', context).once('modularform-people-existing')
        .on('click', '.modularform-people-delete', function () {
          var $li    = $(this).closest('li');
          var value  = $li.data('value');
          var $input = $('#modularform-people-input');

          // Strikethrough
          $li.css('text-decoration', 'line-through').find('.modularform-people-delete').hide();

          // Remove from hidden field
          var current = $input.val().split(',').filter(function (v) {
            return v.trim() !== '' && v.trim() !== value;
          });
          $input.val(current.join(','));
        });

      // ── Select2 for new additions ───────────────────────────────────────
      var $select = $('#modularform-people-select', context);
      if (!$select.length) return;

      $select.once('modularform-select2').select2({
        tags: true,
        tokenSeparators: [','],
        ajax: {
          url: $select.data('ac-url'),
          dataType: 'json',
          quietMillis: 200,
          data: function (term) { return { people: term }; },
          results: function (data) { return { results: data.results }; },
        },
        createSearchChoice: function (term) {
          // Allow raw email entry
          if (/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(term)) {
            return { id: 'email:' + term, text: term };
          }
          return null;
        },
        multiple: true,
      }).on('change', function () {
        var chosen  = $select.select2('data');
        var $input  = $('#modularform-people-input');
        var current = $input.val()
          ? $input.val().split(',').filter(function (v) { return v.trim() !== ''; })
          : [];

        chosen.forEach(function (item) {
          if (current.indexOf(item.id) === -1) {
            current.push(item.id);
          }
        });

        $input.val(current.join(','));
      });
    }
  };
}(jQuery));
