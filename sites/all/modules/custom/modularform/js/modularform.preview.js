(function ($, Drupal) {
  Drupal.behaviors.modularformPreview = {
    attach: function (context, settings) {
      var rules = (settings.modularformPreview && settings.modularformPreview.rules) ? settings.modularformPreview.rules : {};

      // ── Select2 init ───────────────────────────────────────────────────────
      $('.select2-tags-hidden', context).once('select2-tags').each(function () {
        var $hidden = $(this);
        var acUrl = $hidden.data('ac-url');
        var vid = $hidden.data('vid');

        // Insert a visible container div right after the hidden field
        var $container = $('<div class="select2-tags-container"></div>');
        $hidden.after($container);

        // Select2 3.5.4 uses a plain <input type="hidden"> as its anchor
        var $input = $('<input type="hidden" style="width:100%" />');
        $container.append($input);

        $input.select2({
          tags: true,
          multiple: true,
          placeholder: Drupal.t('Search or add tags\u2026'),
          minimumInputLength: 0,

          // Called on init if there's a pre-existing value in $hidden
          initSelection: function (element, callback) {
            var val = $hidden.val();
            if (!val) {
              callback([]);
              return;
            }
            var items = [];
            $.each(val.split(','), function (i, part) {
              part = $.trim(part);
              if (!part) {
                return;
              }
              if (part.indexOf('new:') === 0) {
                var name = part.slice(4);
                items.push({ id: part, text: name });
              } else {
                items.push({ id: part, text: part });
              }
            });
            callback(items);
          },

          // AJAX query against our Drupal callback
          query: function (opts) {
            $.ajax({
              url: acUrl,
              dataType: 'json',
              data: { q: opts.term, page_limit: 20 },
              // no vid
              success: function (data) {
                opts.callback({ results: data.results, more: data.more });
              },
              error: function () {
                opts.callback({ results: [] });
              },
            });
          },

          // FIX 3: use native Array.filter instead of $(data).filter()
          // which is unreliable with plain object arrays in older jQuery
          createSearchChoice: function (term, data) {
            var termLower = term.toLowerCase();
            var match = data.filter(function (item) {
              return item.text.toLowerCase() === termLower;
            });
            if (match.length === 0) {
              return { id: 'new:' + term, text: term + ' (' + Drupal.t('add new') + ')' };
            }
          },

          // Format the display of selected items (strip the "(add new)" suffix)
          formatSelection: function (item) {
            return item.text.replace(/\s*\(add new\)\s*$/, '');
          },
        });

        // Sync Select2 value back to the hidden field Drupal will submit
        // FIX 2: also trigger change on $hidden so validation listeners fire
        $input.on('change', function () {
          var val = $input.select2('val');
          $hidden.val(val ? val.join(',') : '');
          $hidden.trigger('change');
        });
      });

      // ── Client-side validation ─────────────────────────────────────────────
      $.each(rules, function (q_id, q_rules) {
        var $field = $('[data-q-id="' + q_id + '"]', context);

        $field.on('change blur', function () {
          validateField(q_id, q_rules, $field);
        });
      });

      function validateField(q_id, q_rules, $field) {
        var $container = $field.closest('.preview-question');
        clearError($container);

        var q_type = $container.data('q-type');
        var value = getFieldValue(q_type, $container);

        for (var i = 0; i < q_rules.length; i++) {
          var rule = q_rules[i];
          var error = applyRule(rule, value, q_type);
          if (error) {
            showError($container, error);
            return;
          }
        }
      }

      function getFieldValue(q_type, $container) {
        switch (q_type) {
          case 'text':
          case 'textarea':
          case 'date':
          case 'time':
            return $container.find('input, textarea').val() || '';

          case 'richtext':
            var $ta = $container.find('textarea');
            var editor_id = $ta.attr('id');
            if (window.tinyMCE && tinyMCE.get(editor_id)) {
              return tinyMCE.get(editor_id).getContent();
            }
            return $ta.val() || '';

          case 'radio':
          case 'boolean':
            return $container.find('input[type=radio]:checked').val() || '';

          case 'checkbox':
          case 'list':
            var checked = [];
            $container.find('input[type=checkbox]:checked').each(function () {
              checked.push($(this).val());
            });
            return checked;

          case 'dropdown':
            return $container.find('select').val() || '';

          // FIX 1: read from the hidden field and split into array,
          // not from <select> which no longer exists for tag fields
          case 'tag':
            var tagVal = $container.find('.select2-tags-hidden').val();
            if (!tagVal) {
              return [];
            }
            return tagVal.split(',').filter(Boolean);

          default:
            return '';
        }
      }

      function applyRule(rule, value, q_type) {
        var rt = rule.rule_type;
        var rv = rule.rule_value;
        var msg = rule.error_message || defaultMessage(rt, rv);

        var len = typeof value === 'string' ? value.length : 0;
        var words = typeof value === 'string' ? value.trim().split(/\s+/).filter(Boolean).length : 0;
        var count = Array.isArray(value) ? value.length : (value ? 1 : 0);

        switch (rt) {
          case 'min_length':
            return len > 0 && len < parseInt(rv) ? msg : null;
          case 'max_length':
            return len > parseInt(rv) ? msg : null;
          case 'exact_length':
            return len > 0 && len !== parseInt(rv) ? msg : null;
          case 'min_words':
            return words > 0 && words < parseInt(rv) ? msg : null;
          case 'max_words':
            return words > parseInt(rv) ? msg : null;
          case 'is_email':
            return value && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value) ? msg : null;
          case 'is_url':
            return value && !/^https?:\/\/.+/.test(value) ? msg : null;
          case 'is_numeric':
            return value && isNaN(Number(value)) ? msg : null;
          case 'is_alpha':
            return value && !/^[a-zA-Z]+$/.test(value) ? msg : null;
          case 'starts_with':
            return value && value.indexOf(rv) !== 0 ? msg : null;
          case 'ends_with':
            return value && value.slice(-rv.length) !== rv ? msg : null;
          case 'contains':
            return value && value.indexOf(rv) === -1 ? msg : null;
          case 'regex':
            try { return value && !(new RegExp(rv)).test(value) ? msg : null; } catch (e) { return null; }

          case 'date_min':
            return value && value < rv ? msg : null;
          case 'date_max':
            return value && value > rv ? msg : null;
          case 'time_min':
            return value && value < rv ? msg : null;
          case 'time_max':
            return value && value > rv ? msg : null;
          case 'disallow_value':
            return value && value === rv ? msg : null;
          case 'must_be_true':
            return value && value !== 'yes' ? msg : null;
          case 'must_be_false':
            return value && value !== 'no' ? msg : null;

          case 'min_selections':
            return count > 0 && count < parseInt(rv) ? msg : null;
          case 'max_selections':
            return count > parseInt(rv) ? msg : null;
          case 'exact_count':
            return count > 0 && count !== parseInt(rv) ? msg : null;
          case 'min_tags':
            return count > 0 && count < parseInt(rv) ? msg : null;
          case 'max_tags':
            return count > parseInt(rv) ? msg : null;
          case 'exact_tags':
            return count > 0 && count !== parseInt(rv) ? msg : null;
          case 'max_filesize':
            return null;
          case 'min_files':
            return count > 0 && count < parseInt(rv) ? msg : null;
          case 'max_files':
            return count > parseInt(rv) ? msg : null;
          case 'allowed_types':
            return null;

          default:
            return null;
        }
      }

      function defaultMessage(rule_type, rule_value) {
        var map = {
          min_length: 'Must be at least ' + rule_value + ' characters.',
          max_length: 'Must be no more than ' + rule_value + ' characters.',
          exact_length: 'Must be exactly ' + rule_value + ' characters.',
          min_words: 'Must contain at least ' + rule_value + ' words.',
          max_words: 'Must contain no more than ' + rule_value + ' words.',
          is_email: 'Must be a valid email address.',
          is_url: 'Must be a valid URL.',
          is_numeric: 'Must be numeric.',
          is_alpha: 'Must contain letters only.',
          starts_with: 'Must start with "' + rule_value + '".',
          ends_with: 'Must end with "' + rule_value + '".',
          contains: 'Must contain "' + rule_value + '".',
          regex: 'Invalid format.',
          date_min: 'Date must be on or after ' + rule_value + '.',
          date_max: 'Date must be on or before ' + rule_value + '.',
          time_min: 'Time must be at or after ' + rule_value + '.',
          time_max: 'Time must be at or before ' + rule_value + '.',
          disallow_value: 'This option is not allowed.',
          must_be_true: 'Must be Yes.',
          must_be_false: 'Must be No.',
          min_selections: 'Select at least ' + rule_value + ' option(s).',
          max_selections: 'Select no more than ' + rule_value + ' option(s).',
          exact_count: 'Select exactly ' + rule_value + ' option(s).',
          min_tags: 'Add at least ' + rule_value + ' tag(s).',
          max_tags: 'Add no more than ' + rule_value + ' tag(s).',
          exact_tags: 'Add exactly ' + rule_value + ' tag(s).',
          min_files: 'Upload at least ' + rule_value + ' file(s).',
          max_files: 'Upload no more than ' + rule_value + ' file(s).',
        };
        return map[rule_type] || 'Invalid value.';
      }

      function showError($container, message) {
        $container.addClass('preview-question--error');
        $container.find('.preview-inline-error').remove();
        $container.append('<p class="preview-inline-error">' + message + '</p>');
      }

      function clearError($container) {
        $container.removeClass('preview-question--error');
        $container.find('.preview-inline-error').remove();
      }
    }
  };
}(jQuery, Drupal));
