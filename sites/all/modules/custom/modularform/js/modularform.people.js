(function ($) {
  Drupal.behaviors.modularformPeople = {
    attach: function (context, settings) {
      var $list  = $('#modularform-people-existing', context);
      var $count = $('#mf-plist-count', context);
      var $empty = $('#mf-plist-empty', context);
      var label  = (settings.modularformPeople && settings.modularformPeople.peopleLabel)
        ? settings.modularformPeople.peopleLabel
        : 'people';

      function recount() {
        var visible = $list.find('.mf-plist-item:visible:not(.mf-plist-removed)').length;
        $count.text(visible + ' ' + label);
        $empty.toggle($list.find('.mf-plist-item:visible').length === 0);
      }

      // ?? Existing people list ???????????????????????????????????????????????
      $list.once('modularform-people-existing')
        .on('click', '.modularform-people-delete', function () {
          var $li   = $(this).closest('li');
          var value = $li.data('value');
          var $input = $('#modularform-people-input');

          $li.addClass('mf-plist-removed');

          var current = $input.val().split(',').filter(function (v) {
            return v.trim() !== '' && v.trim().indexOf(value) !== 0;
          });
          $input.val(current.join(','));
          recount();
        })
        .on('change', '.modularform-people-role', function () {
          var $li   = $(this).closest('li');
          var value = $li.data('value');
          var role  = $(this).val();
          var $input = $('#modularform-people-input');

          var current = $input.val().split(',').map(function (v) {
            v = v.trim();
            return v.indexOf(value) === 0 ? value + ':' + role : v;
          });
          $input.val(current.join(','));
        });

      // ?? Search ????????????????????????????????????????????????????????????
      $('.mf-plist-search', context).once('mf-plist-search').on('input', function () {
        var term = $(this).val().toLowerCase().trim();

        $list.find('.mf-plist-item').each(function () {
          var label = $(this).data('label') || '';
          $(this).toggle(!term || label.indexOf(term) !== -1);
        });

        recount();
      });

      // ?? Select2 for new additions ?????????????????????????????????????????
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
          if (/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(term)) {
            return { id: 'email:' + term, text: term };
          }
          return null;
        },
        multiple: true,
      }).on('change', function () {
        var chosen = $select.select2('data');
        var $input = $('#modularform-people-input');
        var current = $input.val()
          ? $input.val().split(',').filter(function (v) { return v.trim() !== ''; })
          : [];

        chosen.forEach(function (item) {
          var alreadyIn = current.some(function (v) {
            return v.indexOf(item.id) === 0;
          });
          if (!alreadyIn) {
            current.push(item.id + ':2');  // default role: participant
          }
        });

        $input.val(current.join(','));
      });
    }
  };
}(jQuery));
