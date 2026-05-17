/**
 * @file modularform.response-filter.js
 * Filter bar interaction logic for the form responses view.
 */
(function ($, Drupal) {

  Drupal.behaviors.modularformFilter = {
    attach: function (context, settings) {

      $('#response-filter-bar', context).once('modularform-filter').each(function () {

        // ── Element refs ───────────────────────────────────────────────────

        var $bar          = $(this);
        var $tagArea      = $bar.find('#filter-tags');

        var $stepForm     = $bar.find('#filter-step-form');
        var $formSearch   = $bar.find('#filter-form-search');
        var $formPanel    = $bar.find('#filter-form-panel');
        var $formList     = $bar.find('#filter-form-list');
        var $formChosen   = $bar.find('#filter-form-chosen');

        var $stepQ        = $bar.find('#filter-step-question');
        var $qSearch      = $bar.find('#filter-question-search');
        var $qPanel       = $bar.find('#filter-question-panel');
        var $qList        = $bar.find('#filter-question-list');
        var $qChosen      = $bar.find('#filter-question-chosen');

        var $stepA        = $bar.find('#filter-step-answer');
        var $aSearch      = $bar.find('#filter-answer-search');
        var $aPanel       = $bar.find('#filter-answer-panel');
        var $aList        = $bar.find('#filter-answer-list');
        var $aChosen      = $bar.find('#filter-answer-chosen');

        var $addBtn       = $bar.find('#filter-add-btn');
        var $clearAll     = $bar.find('#filter-clear-all');

        // ── Initial state ──────────────────────────────────────────────────────
        $stepA.addClass('gform-filter-step--locked');

        // ── State ──────────────────────────────────────────────────────────

        var state = {
          form:    null,   // { id, title }
          qKey:    null,   // solr field key string
          qLabel:  null,   // full question text
          aVal:    null,   // chosen answer string
          pairs:   []      // [{ solrKey, qLabel, answer }, …]
        };

        // ── Boot: read URL params and restore state ────────────────────────

        restoreFromUrl();

        function restoreFromUrl() {
          var params = parseQueryString(window.location.search);
          if (!params.form_id) { return; }

          fetchFormById(params.form_id, function (form) {
            if (!form) { return; }
            state.form = form;
            lockFormStep(form);

            // Need question labels — fetch all questions for this form first
            searchQuestions(form.id, '', function () {
              // questionCache[form.id] is now populated
              var i = 0;
              while (params['filters[' + i + '][solr_key]']) {
                var solrKey = params['filters[' + i + '][solr_key]'];
                var answer  = params['filters[' + i + '][answer]'] || '';
                var qLabel  = solrKeyToLabel(solrKey, form.id);
                state.pairs.push({ solrKey: solrKey, qLabel: qLabel, answer: answer });
                i++;
              }
              renderTags();
            });
          });
        }

        // ── Form search ────────────────────────────────────────────────────

        $formSearch.on('input', function () {
          if (state.form) { return; }
          var term = $.trim($(this).val());
          if (!term) { closePanel($formPanel); return; }
          searchForms(term, function (results) {
            populateList($formList, results, function (item) {
              pickForm(item);
            });
            openPanel($formPanel);
          });
        });

        $formSearch.on('focus', function () {
          if (state.form) { return; }
          var term = $.trim($(this).val());
          if (term) { $formPanel.removeAttr('hidden'); }
        });

        // ── Question search ────────────────────────────────────────────────

        $qSearch.on('input', function () {
          if (!state.form || state.qKey) { return; }
          var term = $.trim($(this).val()).toLowerCase();
          searchQuestions(state.form.id, term, function (results) {
            populateList($qList, results, function (item) {
              pickQuestion(item);
            });
            openPanel($qPanel);
          });
        });

        $qSearch.on('focus', function () {
          if (!state.form || state.qKey) { return; }
          searchQuestions(state.form.id, '', function (results) {
            populateList($qList, results, function (item) {
              pickQuestion(item);
            });
            openPanel($qPanel);
          });
        });

        // ── Answer search ──────────────────────────────────────────────────

        $aSearch.on('input', function () {
          if (!state.qKey || state.aVal) { return; }
          var term = $.trim($(this).val()).toLowerCase();
          searchAnswers(state.qKey, term, function (results) {
            populateList($aList, results, function (item) {
              pickAnswer(item);
            });
            openPanel($aPanel);
          });
        });

        $aSearch.on('focus', function () {
          if (!state.qKey || state.aVal) { return; }
          searchAnswers(state.qKey, '', function (results) {
            populateList($aList, results, function (item) {
              pickAnswer(item);
            });
            openPanel($aPanel);
          });
        });

        // ── Add filter ─────────────────────────────────────────────────────

        $addBtn.on('click', function () {
          if (!state.qKey || !state.aVal) { return; }
          state.pairs.push({
            solrKey: state.qKey,
            qLabel:  state.qLabel,
            answer:  state.aVal
          });
          resetQuestionStep();
          resetAnswerStep();
          $addBtn.prop('disabled', true);
          updateUrl();
          reloadResults();
        });

        // ── Clear all ──────────────────────────────────────────────────────

        $clearAll.on('click', function () {
          clearAll();
        });

        // ── Close panels on outside click ──────────────────────────────────

        $(document).on('click.modularform-filter', function (e) {
          if (!$(e.target).closest('#filter-step-form').length)     { closePanel($formPanel); }
          if (!$(e.target).closest('#filter-step-question').length) { closePanel($qPanel); }
          if (!$(e.target).closest('#filter-step-answer').length)   { closePanel($aPanel); }
        });

        // ── Pick handlers ──────────────────────────────────────────────────

        function pickForm(item) {
          // Normalise: search results return { id, label }, fetchFormById returns { id, title }
          state.form = { id: item.id, title: item.title || item.label };
          closePanel($formPanel);
          lockFormStep(state.form);
          unlockStep($stepQ, $qSearch);
          renderTags();
          updateUrl();
        }

        function pickQuestion(item) {
          state.qKey   = item.id;
          state.qLabel = item.label;
          state.aVal   = null;
          closePanel($qPanel);
          showChosen($qSearch, $qChosen, item.label, function () {
            resetQuestionStep();
            resetAnswerStep();
            $addBtn.prop('disabled', true);
          });
          unlockStep($stepA, $aSearch);
          $stepA.removeClass('gform-filter-step--locked');
          // Load answers immediately on question pick
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
              .on('click', function () { clearAll(); })
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
          state.qKey   = null;
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
          state.pairs.splice(index, 1);
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
                $('<span class="gform-filter-tag__label">').text(state.form.title),
                $('<button type="button" class="gform-filter-tag__remove" aria-label="' + Drupal.t('Remove form filter') + '">')
                  .text('×')
                  .on('click', function () { clearAll(); })
              );
            $tagArea.append($formTag);
          }

          $.each(state.pairs, function (i, pair) {
            var shortLabel = pair.qLabel.length > 48
              ? pair.qLabel.slice(0, 45) + '…'
              : pair.qLabel;
            var tagText = shortLabel + ' › ' + pair.answer;

            var $pairTag = $('<span class="gform-filter-tag gform-filter-tag--pair">')
              .append(
                $('<span class="gform-filter-tag__label">').text(tagText),
                $('<button type="button" class="gform-filter-tag__remove">')
                  .attr('aria-label', Drupal.t('Remove filter: @label', { '@label': tagText }))
                  .text('×')
                  .on('click', (function (idx) {
                    return function () { removePair(idx); };
                  }(i)))
              );
            $tagArea.append($pairTag);
          });
        }

        // ── Panel helpers ──────────────────────────────────────────────────

        function openPanel($panel) {
          $panel.removeAttr('hidden');
        }

        function closePanel($panel) {
          $panel.attr('hidden', true);
        }

        function populateList($list, items, onPick) {
          $list.empty();
          if (!items || !items.length) {
            $list.append(
              $('<li class="gform-filter-list--empty">').text(Drupal.t('No matches found'))
            );
            return;
          }
          $.each(items, function (i, item) {
            var label = item.label || item;
            $('<li>').text(label).on('click', function () {
              onPick(item);
            }).appendTo($list);
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
              })
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
            params['filters[' + i + '][answer]']   = pair.answer;
          });

          var qs = $.param(params);
          var newUrl = window.location.pathname + (qs ? '?' + qs : '');

          if (window.history && window.history.pushState) {
            window.history.pushState(null, '', newUrl);
          } else {
            window.location.search = qs;
          }
        }

        // ── Results reload ─────────────────────────────────────────────────

        function reloadResults() {
          window.location.reload();
        }

        // ── Utility ────────────────────────────────────────────────────────

        // Uses indexOf('=') instead of split('=') so values containing encoded
        // '=' characters don't get truncated.
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

        // ── AJAX functions ─────────────────────────────────────────────────

        function searchForms(term, cb) {
          $.ajax({
            url: Drupal.settings.basePath + 'modularform/filter/form-search',
            dataType: 'json',
            data: { term: term },
            success: function (data) { cb(data.results || []); },
            error: function () { cb([]); }
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
            error: function () { cb(null); }
          });
        }

        var questionCache = {};  // keyed by form_id

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
            error: function () { cb([]); }
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
            data: {
              key:     solrKey,
              form_id: state.form.id,
              term:    term
            },
            success: function (data) {
              if (data.error === 'solr_unavailable') {
                cb([]);
                return;
              }
              cb(data.results || []);
            },
            error: function () {
              cb([]);
            }
          });
        }

        function solrKeyToLabel(solrKey, formId) {
          var qs = questionCache[formId] || [];
          var match = $.grep(qs, function (q) { return q.id === solrKey; });
          return match.length ? match[0].label : solrKey;
        }

      }); // end .once()
    }
  };

}(jQuery, Drupal));
