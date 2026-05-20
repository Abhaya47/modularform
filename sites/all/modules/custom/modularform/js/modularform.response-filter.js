/**
 * @file modularform.response-filter.js
 * Filter bar interaction logic for the form responses view.
 */
(function ($, Drupal) {

  Drupal.behaviors.modularformFilter = {
    attach: function (context, settings) {

      $('#response-filter-bar', context).once('modularform-filter').each(function () {

        // ── Element refs ───────────────────────────────────────────────────

        var $bar = $(this);
        var $tagArea = $bar.find('#filter-tags');

        var $stepForm = $bar.find('#filter-step-form');
        var $formSearch = $bar.find('#filter-form-search');
        var $formPanel = $bar.find('#filter-form-panel');
        var $formList = $bar.find('#filter-form-list');
        var $formChosen = $bar.find('#filter-form-chosen');

        var $stepQ = $bar.find('#filter-step-question');
        var $qSearch = $bar.find('#filter-question-search');
        var $qPanel = $bar.find('#filter-question-panel');
        var $qList = $bar.find('#filter-question-list');
        var $qChosen = $bar.find('#filter-question-chosen');

        var $stepA = $bar.find('#filter-step-answer');
        var $aSearch = $bar.find('#filter-answer-search');
        var $aPanel = $bar.find('#filter-answer-panel');
        var $aList = $bar.find('#filter-answer-list');
        var $aChosen = $bar.find('#filter-answer-chosen');

        var $addBtn = $bar.find('#filter-add-btn');
        var $clearAll = $bar.find('#filter-clear-all');

        // ── Initial state ──────────────────────────────────────────────────

        $stepQ.addClass('gform-filter-step--locked');
        $qSearch.prop('disabled', true);
        $stepA.addClass('gform-filter-step--locked');

        var state = {
          form: null,
          qKey: null,
          qLabel: null,
          aVal: null,
          pairs: [],
        };

        // ── Boot ──────────────────────────────────────────────────────────

        restoreFromUrl();

        function restoreFromUrl() {
          var params = parseQueryString(window.location.search);
          if (!params.form_id) { return; }

          fetchFormById(params.form_id, function (form) {
            if (!form) { return; }
            state.form = form;
            state.form.total = params.form_total || null;
            lockFormStep(form);
            unlockStep($stepQ, $qSearch);

            searchQuestions(form.id, '', function () {
              var i = 0;
              while (params['filters[' + i + '][solr_key]']) {
                var solrKey = params['filters[' + i + '][solr_key]'];
                var answer = params['filters[' + i + '][answer]'] || '';
                var qLabel = solrKeyToLabel(solrKey, form.id);
                state.pairs.push({
                  solrKey: solrKey,
                  qLabel: qLabel,
                  answer: answer,
                  count: null,   // will be populated by first reloadResults()
                });
                i++;
              }

              if (state.pairs.length) {
                var $count = $('.gform-table-count');
                if ($count.length) {
                  state.pairs[state.pairs.length - 1].count = $.trim($count.text());
                }
              }

              renderTags();
            });
          });
        }

        // ── Form search ────────────────────────────────────────────────────

        $formSearch.on('input', function () {
          if (state.form) { return; }
          var term = $.trim($(this).val());
          if (!term) {
            closePanel($formPanel);
            return;
          }
          searchForms(term, function (results) {
            populateList($formList, results, function (item) { pickForm(item); });
            openPanel($formPanel);
          });
        });

        $formSearch.on('focus', function () {
          if (state.form) { return; }
          if ($.trim($(this).val())) { $formPanel.removeAttr('hidden'); }
        });

        // ── Question search ────────────────────────────────────────────────

        $qSearch.on('input', function () {
          if (!state.form || state.qKey) { return; }
          var term = $.trim($(this).val()).toLowerCase();
          searchQuestions(state.form.id, term, function (results) {
            populateList($qList, results, function (item) { pickQuestion(item); });
            openPanel($qPanel);
          });
        });

        $qSearch.on('focus', function () {
          if (!state.form || state.qKey) { return; }
          searchQuestions(state.form.id, '', function (results) {
            populateList($qList, results, function (item) { pickQuestion(item); });
            openPanel($qPanel);
          });
        });

        // ── Answer search ──────────────────────────────────────────────────

        $aSearch.on('input', function () {
          if (!state.qKey || state.aVal) { return; }
          var term = $.trim($(this).val()).toLowerCase();
          searchAnswers(state.qKey, term, function (results) {
            populateList($aList, results, function (item) { pickAnswer(item); });
            openPanel($aPanel);
          });
        });

        $aSearch.on('focus', function () {
          if (!state.qKey || state.aVal) { return; }
          searchAnswers(state.qKey, '', function (results) {
            populateList($aList, results, function (item) { pickAnswer(item); });
            openPanel($aPanel);
          });
        });

        // ── Add filter ─────────────────────────────────────────────────────

        $addBtn.on('click', function () {
          if (!state.form) { return; }
          if (state.qKey && state.aVal) {
            state.pairs.push({
              solrKey: state.qKey,
              qLabel: state.qLabel,
              answer: state.aVal,
              count: null,
            });
          }
          resetQuestionStep();
          resetAnswerStep();
          $addBtn.prop('disabled', true);
          updateUrl();
          reloadResults();
        });

        // ── Clear all ──────────────────────────────────────────────────────

        $clearAll.on('click', function () { clearAll(); });

        // ── Close panels on outside click ──────────────────────────────────

        $(document).on('click.modularform-filter', function (e) {
          if (!$(e.target).closest('#filter-step-form').length) { closePanel($formPanel); }
          if (!$(e.target).closest('#filter-step-question').length) { closePanel($qPanel); }
          if (!$(e.target).closest('#filter-step-answer').length) { closePanel($aPanel); }
        });

        // ── Configured filter bridge ───────────────────────────────────────

        $(document).on('configuredFilterApplied', function (e, pair) {
          state.pairs.push({
            solrKey: pair.solrKey,
            qLabel: pair.qLabel,
            answer: pair.answer,
            count: null,
          });
          renderTags();
          updateUrl();
          reloadResults();
        });

        $(document).on('configuredFilterRemoved', function (e, pair) {
          state.pairs = $.grep(state.pairs, function (p) {
            return !(p.solrKey === pair.solrKey && p.answer === pair.answer);
          });
          renderTags();
          updateUrl();
          reloadResults();
        });

        // email

        $(document).on('emailFilterChanged', function () {
          updateUrl();
          reloadResults();
        });

        // ── Pick handlers ──────────────────────────────────────────────────

        function pickForm(item) {
          state.form = { id: item.id, title: item.title || item.label, total: null };
          closePanel($formPanel);
          lockFormStep(state.form);
          unlockStep($stepQ, $qSearch);
          $addBtn.prop('disabled', false);

          $('#configured-filters-toggle').removeClass('gform-quick-filter--active');
          $('#configured-filters-dropdown').removeClass('open').removeData('loaded').empty();

          renderTags();
          updateUrl();
          reloadResults();
        }

        function pickQuestion(item) {
          state.qKey = item.id;
          state.qLabel = item.label;
          state.aVal = null;
          closePanel($qPanel);
          showChosen($qSearch, $qChosen, item.label, function () {
            resetQuestionStep();
            resetAnswerStep();
            $addBtn.prop('disabled', true);
          });
          unlockStep($stepA, $aSearch);
          $stepA.removeClass('gform-filter-step--locked');
          searchAnswers(item.id, '', function (results) {
            populateList($aList, results, function (a) { pickAnswer(a); });
            openPanel($aPanel);
          });
        }

        function pickAnswer(item) {
          state.aVal = item.label || item;
          closePanel($aPanel);
          showChosen($aSearch, $aChosen, state.aVal, function () {
            state.aVal = null;
            $addBtn.prop('disabled', true);
          });
          $addBtn.prop('disabled', false);
        }

        // ── Step helpers ───────────────────────────────────────────────────

        function lockFormStep(form) {
          $formSearch.val('').prop('disabled', true).hide();
          $formChosen.show().empty().append(
            $('<span class="gform-filter-chosen__text">').text(form.title),
            $('<button type="button" class="gform-filter-chosen__change">')
              .text(Drupal.t('Change'))
              .on('click', function () { clearAll(); }),
          );
        }

        function unlockStep($step, $input) {
          $step.removeAttr('hidden').removeClass('gform-filter-step--locked');
          $input.prop('disabled', false);
        }

        function lockStep($step, $input) {
          $step.attr('hidden', true).addClass('gform-filter-step--locked');
          $input.prop('disabled', true).val('');
        }

        function resetQuestionStep() {
          state.qKey = null;
          state.qLabel = null;
          $qSearch.val('').show().prop('disabled', false);
          $qChosen.hide().empty();
          closePanel($qPanel);
        }

        function resetAnswerStep() {
          state.aVal = null;
          $aSearch.val('').show().prop('disabled', true);
          $aChosen.hide().empty();
          closePanel($aPanel);
          lockStep($stepA, $aSearch);
          $stepA.addClass('gform-filter-step--locked');
        }

        function clearAll() {
          state = { form: null, qKey: null, qLabel: null, aVal: null, pairs: [] };

          // Clear tags immediately, don't wait for AJAX
          renderTags();

          $('#configured-filters-toggle').removeClass('gform-quick-filter--active');
          $('#configured-filters-dropdown').removeClass('open').removeData('loaded').empty();
          // Reset configured-filter checkboxes
          $('#configured-filters-dropdown .cf-answer-item').each(function () {
            $(this).find('input[type="checkbox"]').prop('checked', false);
            $(this).removeClass('is-checked');
          });
          $('#configured-filters-dropdown .cf-field-badge').text('0');
          $('#configured-filters-dropdown .cf-question-card').removeClass('has-selection');

          $formSearch.val('').prop('disabled', false).show();
          $formChosen.hide().empty();
          closePanel($formPanel);

          lockStep($stepQ, $qSearch);
          $stepQ.addClass('gform-filter-step--locked');
          $qChosen.hide().empty();
          closePanel($qPanel);

          lockStep($stepA, $aSearch);
          $stepA.addClass('gform-filter-step--locked');
          $aChosen.hide().empty();
          closePanel($aPanel);

          $addBtn.prop('disabled', true);
          updateUrl();
          reloadResults();
        }

        function removePair(index) {
          var pair = state.pairs[index];

          // Uncheck the matching checkbox in the configured-filters dropdown
          $('#configured-filters-dropdown .cf-question-card').each(function () {
            if ($(this).data('solr-key') === pair.solrKey) {
              $(this).find('.cf-answer-list li.cf-answer-item').each(function () {
                if ($(this).find('span').text() === pair.answer) {
                  var $cb = $(this).find('input[type="checkbox"]');
                  $cb.prop('checked', false);
                  $(this).removeClass('is-checked');
                }
              });
              // Update badge
              var count = $(this).find('input:checked').length;
              $(this).find('.cf-field-badge').text(count);
              $(this).toggleClass('has-selection', count > 0);
            }
          });

          state.pairs.splice(index, 1);
          renderTags();
          updateUrl();
          reloadResults();
        }

        // ── Tag rendering ──────────────────────────────────────────────────

        function renderTags() {
          $tagArea.empty();
          if (!state.form && !state.pairs.length) { return; }

          if (state.form) {
            var $formTag = $('<span class="gform-filter-tag gform-filter-tag--form">')
              .append(
                $('<span class="gform-filter-tag__label">').text(state.form.title + ' (' + (state.form.total || '?') + ' responses)'),
                $('<button type="button" class="gform-filter-tag__remove">')
                  .attr('aria-label', Drupal.t('Remove form filter'))
                  .text('×')
                  .on('click', function () { clearAll(); }),
              );
            $tagArea.append($formTag);
          }

          $.each(state.pairs, function (i, pair) {
            var shortLabel = pair.qLabel.length > 48 ? pair.qLabel.slice(0, 45) + '…' : pair.qLabel;
            var countSuffix = pair.count ? ' (' + pair.count + ')' : '';
            var tagText = shortLabel + ' › ' + pair.answer + countSuffix;
            var $pairTag = $('<span class="gform-filter-tag gform-filter-tag--pair">')
              .append(
                $('<span class="gform-filter-tag__label">').text(tagText),
                $('<button type="button" class="gform-filter-tag__remove">')
                  .attr('aria-label', Drupal.t('Remove filter: @label', { '@label': tagText }))
                  .text('×')
                  .on('click', (function (idx) {
                    return function () { removePair(idx); };
                  }(i))),
              );
            $tagArea.append($pairTag);
          });

          var emailParams = parseQueryString(window.location.search);
          if (emailParams.email) {
            var $emailTag = $('<span class="gform-filter-tag gform-filter-tag--pair" id="tag-email">')
              .append(
                $('<span class="gform-filter-tag__label">').text('Email: ' + emailParams.email),
                $('<button type="button" class="gform-filter-tag__remove">')
                  .attr('aria-label', Drupal.t('Remove email filter'))
                  .text('×')
                  .on('click', function () {
                    var p = parseQueryString(window.location.search);
                    delete p.email;
                    var qs = $.param(p);
                    window.history.pushState(null, '', window.location.pathname + (qs ? '?' + qs : ''));
                    // Sync the quick filter chip UI
                    $('#quick-filter-email').removeClass('gform-quick-filter--active');
                    $('#quick-filter-email-value').val('');
                    $('#quick-filter-email-input').hide();
                    $(document).trigger('emailFilterChanged');
                  }),
              );
            $tagArea.append($emailTag);
          }
        }

        // ── Panel helpers ──────────────────────────────────────────────────

        function openPanel($panel) { $panel.removeAttr('hidden'); }

        function closePanel($panel) { $panel.attr('hidden', true); }

        function populateList($list, items, onPick) {
          $list.empty();
          if (!items || !items.length) {
            $list.append($('<li class="gform-filter-list--empty">').text(Drupal.t('No matches found')));
            return;
          }
          $.each(items, function (i, item) {
            $('<li>').text(item.label || item).on('click', function () { onPick(item); }).appendTo($list);
          });
        }

        function showChosen($input, $chosen, text, onClear) {
          $input.val('').hide();
          $chosen.show().empty().append(
            $('<span class="gform-filter-chosen__text">').text(text),
            $('<button type="button" class="gform-filter-chosen__change">')
              .text(Drupal.t('Change'))
              .on('click', function () {
                $chosen.hide().empty();
                $input.val('').show().prop('disabled', false).trigger('focus');
                if (typeof onClear === 'function') { onClear(); }
              }),
          );
        }

        // ── URL management ─────────────────────────────────────────────────

        function updateUrl() {
          var params = {};
          if (state.form) {
            params.form_id = state.form.id;
          }
          $.each(state.pairs, function (i, pair) {
            params['filters[' + i + '][solr_key]'] = pair.solrKey;
            params['filters[' + i + '][answer]'] = pair.answer;
          });
          var qs = $.param(params);
          var newUrl = window.location.pathname + (qs ? '?' + qs : '');
          if (window.history && window.history.pushState) {
            window.history.pushState(null, '', newUrl);
          } else {
            window.location.search = qs;
          }
        }

        function reloadResults() {
          var $card = $('.gform-card');
          var $pager = $('.gform-back-link');

          $card.css('opacity', 0.4);

          var ajaxUrl = Drupal.settings.basePath + 'forms/response/list/ajax';

          $.ajax({
            url: ajaxUrl,
            data: parseQueryString(window.location.search),
            dataType: 'json',
            success: function (data) {
              if (!data || !data.html) {
                $card.css('opacity', '');
                return;
              }
              var $new = $(data.html);
              var $newCard = $new.filter('.gform-card').add($new.find('.gform-card')).first();
              var $newPager = $new.filter('.pager').add($new.find('.pager')).first();
              var $newCount = $newCard.find('.gform-table-count');

              if ($newCount.length) {
                var countText = $.trim($newCount.text());
                var m = countText.match(/(\d[\d,]*)/);
                var count = m ? m[1] : null;

                if (state.form && state.form.total === null) {
                  // Only set once — this is the unfiltered total
                  state.form.total = count;
                }

                // Stamp the count on the last pair
                if (state.pairs.length) {
                  state.pairs[state.pairs.length - 1].count = count;
                }

                renderTags();
              }

              $card.css('opacity', '');
              Drupal.attachBehaviors();
            },
            error: function (xhr, status, err) {
              $card.css('opacity', '');
              console.error('reloadResults failed:', ajaxUrl, status, err, xhr.responseText);
              // Don't reload — just leave the current results in place
            },
          });
        }

        // ── Utility ────────────────────────────────────────────────────────

        function parseQueryString(search) {
          var params = {};
          var str = search.replace(/^\?/, '');
          if (!str) { return params; }
          $.each(str.split('&'), function (i, part) {
            var eq = part.indexOf('=');
            if (eq === -1) { return; }
            var key = decodeURIComponent(part.slice(0, eq).replace(/\+/g, ' '));
            var val = decodeURIComponent(part.slice(eq + 1).replace(/\+/g, ' '));
            params[key] = val;
          });
          return params;
        }

        // ── AJAX ──────────────────────────────────────────────────────────

        function searchForms(term, cb) {
          $.ajax({
            url: Drupal.settings.basePath + 'modularform/filter/form-search',
            dataType: 'json',
            data: { term: term },
            success: function (data) { cb(data.results || []); },
            error: function () { cb([]); },
          });
        }

        function fetchFormById(id, cb) {
          $.ajax({
            url: Drupal.settings.basePath + 'modularform/filter/form-search',
            dataType: 'json',
            data: { id: id },
            success: function (data) {
              var r = data.results || [];
              cb(r.length ? { id: r[0].id, title: r[0].label } : null);
            },
            error: function () { cb(null); },
          });
        }

        var questionCache = {};

        function searchQuestions(formId, term, cb) {
          if (questionCache[formId]) {
            cb(filterByTerm(questionCache[formId], term));
            return;
          }
          $.ajax({
            url: Drupal.settings.basePath + 'modularform/filter/questions',
            dataType: 'json',
            data: { form_id: formId },
            success: function (data) {
              questionCache[formId] = data.results || [];
              cb(filterByTerm(questionCache[formId], term));
            },
            error: function () { cb([]); },
          });
        }

        function filterByTerm(items, term) {
          if (!term) { return items; }
          var lower = term.toLowerCase();
          return $.grep(items, function (item) {
            return item.label.toLowerCase().indexOf(lower) !== -1;
          });
        }

        function searchAnswers(solrKey, term, cb) {
          $.ajax({
            url: Drupal.settings.basePath + 'modularform/filter/answer-values',
            dataType: 'json',
            data: { key: solrKey, form_id: state.form.id, term: term },
            success: function (data) {
              if (data.error === 'solr_unavailable') {
                cb([]);
                return;
              }
              cb(data.results || []);
            },
            error: function () { cb([]); },
          });
        }

        function solrKeyToLabel(solrKey, formId) {
          var qs = questionCache[formId] || [];
          var match = $.grep(qs, function (q) { return q.id === solrKey; });
          return match.length ? match[0].label : solrKey;
        }

      }); // end .once()
    },
  };

  // ── Sort ──────────────────────────────────────────────────────────────────

  Drupal.behaviors.modularformSort = {
    attach: function (context, settings) {
      $('#mf-filter-sort', context).once('modularform-sort').on('change', function () {
        var val = $(this).val();
        var $tbody = $('table.gform-table tbody');
        if (!$tbody.length) { return; }

        var $rows = $tbody.find('tr:not(.gform-empty-row)').get();
        if (!$rows.length) { return; }

        $rows.sort(function (a, b) {
          var aVal, bVal;

          if (val === 'created_asc' || val === 'created_desc') {
            aVal = new Date($.trim($(a).find('td:nth-child(3)').text()));
            bVal = new Date($.trim($(b).find('td:nth-child(3)').text()));
            return val === 'created_asc' ? aVal - bVal : bVal - aVal;
          }

          if (val === 'respondent_asc' || val === 'respondent_desc') {
            aVal = $.trim($(a).find('td.gform-td-name .gform-name-text').text()).toLowerCase();
            bVal = $.trim($(b).find('td.gform-td-name .gform-name-text').text()).toLowerCase();
            if (aVal < bVal) { return val === 'respondent_asc' ? -1 : 1; }
            if (aVal > bVal) { return val === 'respondent_asc' ? 1 : -1; }
            return 0;
          }

          return 0;
        });

        $.each($rows, function (i, row) { $tbody.append(row); });
      });
    },
  };

  // ── Quick filters (email) ─────────────────────────────────────────────────

  Drupal.behaviors.modularformQuickFilters = {
    attach: function (context, settings) {
      var $chip = $('#quick-filter-email', context).once('quick-filter-email');
      var $inputWrap = $('#quick-filter-email-input');
      var $input = $('#quick-filter-email-value');
      var $apply = $('#quick-filter-email-apply');
      var $clear = $('#quick-filter-email-clear');
      if (!$chip.length) { return; }

      var params = parseQS(window.location.search);
      if (params.email) {
        $input.val(params.email);
        $chip.addClass('gform-quick-filter--active');
        $inputWrap.show();
      }

      $chip.on('click', function () {
        if ($inputWrap.is(':visible')) { $inputWrap.hide(); } else {
          $inputWrap.show();
          $input.trigger('focus');
        }
      });

      $apply.on('click', function () {
        var val = $.trim($input.val());
        if (!val) { return; }
        $chip.addClass('gform-quick-filter--active');
        pushEmail(val);
      });

      $input.on('keydown', function (e) {
        if (e.which === 13) { $apply.trigger('click'); }
      });

      $clear.on('click', function () { clearEmail(); });

      function pushEmail(val) {
        var p = parseQS(window.location.search);
        p.email = val;
        var qs = $.param(p);
        window.history.pushState(null, '', window.location.pathname + '?' + qs);
        $chip.addClass('gform-quick-filter--active');
        $(document).trigger('emailFilterChanged');
      }

      function clearEmail() {
        $chip.removeClass('gform-quick-filter--active');
        $input.val('');
        $inputWrap.hide();
        var p = parseQS(window.location.search);
        delete p.email;
        var qs = $.param(p);
        window.history.pushState(null, '', window.location.pathname + (qs ? '?' + qs : ''));
        $(document).trigger('emailFilterChanged');
      }

      function parseQS(search) {
        var params = {};
        var str = search.replace(/^\?/, '');
        if (!str) { return params; }
        $.each(str.split('&'), function (i, part) {
          var eq = part.indexOf('=');
          if (eq === -1) { return; }
          params[decodeURIComponent(part.slice(0, eq).replace(/\+/g, ' '))] =
            decodeURIComponent(part.slice(eq + 1).replace(/\+/g, ' '));
        });
        return params;
      }
    },
  };

  // ── Configured filters ────────────────────────────────────────────────────

  // ── Configured filters ────────────────────────────────────────────────────

  // ── Configured filters ────────────────────────────────────────────────────

  // ── Configured filters ────────────────────────────────────────────────────

  Drupal.behaviors.configuredFilters = {
    attach: function (context, settings) {

      $('#configured-filters-toggle', context).once('configured-filters').on('click', function () {
        var $dropdown = $('#configured-filters-dropdown');
        var $btn = $(this);
        var opening = !$dropdown.hasClass('open');
        $dropdown.toggleClass('open');
        $btn.toggleClass('gform-quick-filter--active');
        if (opening && !$dropdown.data('loaded')) {
          _loadConfiguredFilters($dropdown);
        }
      });

      // Close all card dropdowns when clicking outside
      $(document).on('click.cf-outside', function (e) {
        if (!$(e.target).closest('.cf-question-card').length) {
          $('.cf-question-card').removeClass('open');
        }
      });
    },
  };

  function _loadConfiguredFilters($dropdown) {
    var formId = _getFormId();
    if (!formId) {
      $dropdown.html('<span class="configured-filter-empty">' + Drupal.t('Select a form first.') + '</span>');
      $dropdown.data('loaded', true);
      return;
    }
    $dropdown.html('<span class="configured-filter-empty">' + Drupal.t('Loading\u2026') + '</span>');

    $.getJSON(Drupal.settings.basePath + 'modularform/filter/questions', { form_id: formId, configured: 1 })
      .done(function (data) {
        $dropdown.empty();

        if (!data.results || !data.results.length) {
          $dropdown.html('<span class="configured-filter-empty">' + Drupal.t('No filterable questions configured.') + '</span>');
          $dropdown.data('loaded', true);
          return;
        }

        var pending = data.results.length;

        $.each(data.results, function (i, q) {
          var $card = $(
            '<div class="cf-question-card" data-solr-key="' + q.id + '">' +
            '<div class="cf-question-label">' + q.label + '</div>' +
            '<div class="cf-field-wrap">' +
            '<input type="text" class="cf-field-input" placeholder="' + Drupal.t('Select\u2026') + '" autocomplete="off" />' +
            '<span class="cf-field-badge">0</span>' +
            '<i class="ti ti-chevron-down cf-field-chevron" aria-hidden="true"></i>' +
            '</div>' +
            '<div class="cf-question-panel">' +
            '<ul class="cf-answer-list"><li class="cf-answer-empty">' + Drupal.t('Loading\u2026') + '</li></ul>' +
            '</div>' +
            '</div>',
          );

          var $input = $card.find('.cf-field-input');
          var $badge = $card.find('.cf-field-badge');
          var $panel = $card.find('.cf-question-panel');
          var $list = $card.find('.cf-answer-list');

          // Open on focus or click
          $input.on('focus click', function (e) {
            e.stopPropagation();
            var wasOpen = $card.hasClass('open');
            $('.cf-question-card').not($card).removeClass('open');
            if (!wasOpen) { $card.addClass('open'); }
          });

          // Live filter — type to narrow the list, dropdown stays open
          $input.on('input', function () {
            var term = $(this).val().toLowerCase();
            $list.find('li.cf-answer-item').each(function () {
              var text = $(this).find('span').text().toLowerCase();
              $(this).toggle(text.indexOf(term) !== -1);
            });
            var hasVisible = $list.find('li.cf-answer-item:visible').length > 0;
            $list.find('li.cf-answer-empty').toggle(!hasVisible);
            $card.addClass('open');
          });

          // Clicking a row toggles its checkbox, keeps panel open
          $list.on('click', 'li.cf-answer-item', function (e) {
            e.stopPropagation();
            var $cb = $(this).find('input[type="checkbox"]');
            $cb.prop('checked', !$cb.prop('checked')).trigger('change');
          });

          // Checkbox change — update badge + fire events
          $list.on('change', 'input[type="checkbox"]', function () {
            var $cb = $(this);
            var $li = $cb.closest('li');
            var isSelectAll = $cb.hasClass('cf-select-all');

            if (isSelectAll) {
              var checked = $cb.is(':checked');
              // Toggle all visible answer items
              $list.find('li.cf-answer-item:not(.cf-answer-select-all):visible').each(function () {
                var $itemCb = $(this).find('input[type="checkbox"]');
                if ($itemCb.prop('checked') !== checked) {
                  $itemCb.prop('checked', checked).trigger('change');
                }
              });
              return;
            }

            var checked = $cb.is(':checked');
            var answer = $li.find('span').text();

            $li.toggleClass('is-checked', checked);

            var $allItems = $list.find('li.cf-answer-item:not(.cf-answer-select-all)');
            var $checkedItems = $allItems.find('input:checked');

            // Sync select-all state
            var $selectAllCb = $list.find('.cf-select-all');
            if ($checkedItems.length === 0) {
              $selectAllCb.prop({ checked: false, indeterminate: false });
            } else if ($checkedItems.length === $allItems.length) {
              $selectAllCb.prop({ checked: true, indeterminate: false });
            } else {
              $selectAllCb.prop({ checked: false, indeterminate: true });
            }

            var count = $checkedItems.length;
            $badge.text(count);
            $card.toggleClass('has-selection', count > 0);

            if (checked) {
              $(document).trigger('configuredFilterApplied', [{ solrKey: q.id, qLabel: q.label, answer: answer }]);
            } else {
              $(document).trigger('configuredFilterRemoved', [{ solrKey: q.id, answer: answer }]);
            }
          });

          $dropdown.append($card);

          // Fetch answer values for this question
          $.getJSON(Drupal.settings.basePath + 'modularform/filter/answer-values', {
            key: q.id,
            form_id: formId,
          })
            .done(function (adata) {
              $list.empty();
              var results = adata.results || [];

              if (!results.length) {
                $list.html('<li class="cf-answer-empty">' + Drupal.t('No values.') + '</li>');
                return;
              }

              // Select all row
              var $selectAll = $(
                '<li class="cf-answer-item cf-answer-select-all">' +
                '<input type="checkbox" class="cf-select-all" />' +
                '<span>' + Drupal.t('Select all') + '</span>' +
                '</li>',
              );
              $list.append($selectAll);

              $.each(results, function (j, item) {
                $list.append(
                  '<li class="cf-answer-item">' +
                  '<input type="checkbox" />' +
                  '<span>' + item.label + '</span>' +
                  '</li>',
                );
              });

              // Always keep a hidden "no matches" row for when search filters everything out
              $list.append('<li class="cf-answer-empty" style="display:none">' + Drupal.t('No matches.') + '</li>');
            })
            .fail(function () {
              $list.html('<li class="cf-answer-empty">' + Drupal.t('Failed to load.') + '</li>');
            })
            .always(function () {
              pending--;
              if (pending === 0) { $dropdown.data('loaded', true); }
            });
        });
      })
      .fail(function () {
        $dropdown.html('<span class="configured-filter-empty">' + Drupal.t('Failed to load filters.') + '</span>');
      });
  }

  function _getFormId() {
    var match = window.location.search.match(/form_id=(\d+)/);
    return match ? parseInt(match[1]) : 0;
  }

}(jQuery, Drupal));
