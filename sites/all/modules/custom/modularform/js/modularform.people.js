(function ($) {
  Drupal.behaviors.modularformPeople = {
    attach: function (context, settings) {

      // ── Existing people list — delete on click ──────────────────────────
      $('#modularform-people-existing', context).once('modularform-people-existing')
        .on('click', '.modularform-people-delete', function () {
          var $li   = $(this).closest('li');
          var value = $li.data('value');  // still uid:X or email:Y (without role)
          var $input = $('#modularform-people-input');
          $li.css('text-decoration', 'line-through').find('.modularform-people-delete, .modularform-people-role').hide();
          // Remove any entry that starts with this value
          var current = $input.val().split(',').filter(function (v) {
            return v.trim() !== '' && v.trim().indexOf(value) !== 0;
          });
          $input.val(current.join(','));
        })
        // Role change handler — update the role suffix in hidden field
        .on('change', '.modularform-people-role', function () {
          var $li    = $(this).closest('li');
          var value  = $li.data('value');  // uid:X or email:Y
          var role   = $(this).val();
          var $input = $('#modularform-people-input');
          var current = $input.val().split(',').map(function (v) {
            v = v.trim();
            if (v.indexOf(value) === 0) {
              return value + ':' + role;  // replace role suffix
            }
            return v;
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
          var withRole = item.id + ':2';  // default role: participant
          var alreadyIn = current.some(function (v) {
            return v.indexOf(item.id) === 0;
          });
          if (!alreadyIn) {
            current.push(withRole);
          }
        });
        $input.val(current.join(','));
      });
    }
  };
}(jQuery));
