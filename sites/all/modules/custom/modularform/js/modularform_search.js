(function ($, Drupal) {
  'use strict';

  // -------------------------------------------------------------------------
  // State
  // -------------------------------------------------------------------------
  var state = {
    form_label   : null,   // string
    form_title   : null,   // string (display)
    questions    : [],     // [{ q_id, label, type, options[] }]
    active_filters: [],    // [{ label, value, type, display }]
    page         : 0,
    total        : 0,
    any_editable : false,
    loading      : false,
  };

  // -------------------------------------------------------------------------
  // DOM refs (populated on attach)
  // -------------------------------------------------------------------------
  var $wrap, $formInput, $formSuggestions, $questionDropdown,
    $filterChips, $valueWrap, $valueInput, $valueSelect,
    $addFilterBtn, $tableWrap, $pagerWrap, $statusBar;

  // -------------------------------------------------------------------------
  // Attach
  // -------------------------------------------------------------------------
  Drupal.behaviors.modularformSearch = {
    attach: function (context) {
      $wrap = $('#modularform-search-ui', context);
      if (!$wrap.length) return;

      $formInput       = $wrap.find('.mfs-form-input');
      $formSuggestions = $wrap.find('.mfs-form-suggestions');
      $questionDropdown= $wrap.find('.mfs-question-select');
      $filterChips     = $wrap.find('.mfs-filter-chips');
      $valueWrap       = $wrap.find('.mfs-value-wrap');
      $valueInput      = $wrap.find('.mfs-value-input');
      $valueSelect     = $wrap.find('.mfs-value-select');
      $addFilterBtn    = $wrap.find('.mfs-add-filter');
      $tableWrap       = $('#modularform-response-table-wrap', context);
      $pagerWrap       = $('#modularform-response-pager', context);
      $statusBar       = $wrap.find('.mfs-status');

      bindEvents();
    }
  };

  // -------------------------------------------------------------------------
  // Events
  // -------------------------------------------------------------------------
  function bindEvents() {
    // Form autocomplete typing
    $formInput.on('input', debounce(onFormInput, 250));
    $formInput.on('keydown', function (e) {
      if (e.key === 'Escape') $formSuggestions.hide();
    });
    $(document).on('click', function (e) {
      if (!$(e.target).closest('.mfs-form-autocomplete').length) {
        $formSuggestions.hide();
      }
    });

    // Question select
    $questionDropdown.on('change', onQuestionChange);

    // Value controls
    $addFilterBtn.on('click', onAddFilter);
    $valueInput.on('keydown', function (e) {
      if (e.key === 'Enter') onAddFilter();
    });

    // Pager
    $pagerWrap.on('click', '.mfs-pager-btn', function () {
      var p = parseInt($(this).data('page'), 10);
      if (!isNaN(p)) {
        state.page = p;
        fetchResponses();
      }
    });
  }

  // -------------------------------------------------------------------------
  // Form autocomplete
  // -------------------------------------------------------------------------
  function onFormInput() {
    var q = $formInput.val().trim();
    if (q.length < 1) {
      $formSuggestions.hide().empty();
      return;
    }

    $.getJSON(Drupal.settings.modularformSearch.autocompleteUrl, { term: q })
      .done(function (results) {
        $formSuggestions.empty();
        if (!results.length) {
          $formSuggestions.hide();
          return;
        }
        $.each(results, function (_, r) {
          $('<div class="mfs-suggestion-item">').text(r.title)
            .data('form', r)
            .appendTo($formSuggestions)
            .on('click', function () { selectForm($(this).data('form')); });
        });
        $formSuggestions.show();
      });
  }

  function selectForm(form) {
    state.form_label = form.label;
    state.form_title = form.title;
    state.active_filters = [];
    state.page = 0;

    $formInput.val(form.title);
    $formSuggestions.hide().empty();
    $filterChips.empty();
    resetValueControls();

    setStatus(Drupal.t('Loading questions…'));
    fetchQuestions(form.label);
  }

  // -------------------------------------------------------------------------
  // Questions
  // -------------------------------------------------------------------------
  function fetchQuestions(form_label) {
    $.ajax({
      url        : Drupal.settings.modularformSearch.questionsUrl,
      method     : 'POST',
      contentType: 'application/json',
      data       : JSON.stringify({ form_label: form_label }),
    })
      .done(function (data) {
        state.questions = data.questions || [];
        populateQuestionDropdown(state.questions);
        setStatus('');
        fetchResponses(); // show all responses for this form immediately
      })
      .fail(function () {
        setStatus(Drupal.t('Failed to load questions.'), true);
      });
  }

  function populateQuestionDropdown(questions) {
    $questionDropdown.empty().append(
      $('<option value="">').text('— ' + Drupal.t('Add a question filter') + ' —')
    );
    $.each(questions, function (_, q) {
      $('<option>').val(q.q_id).text(q.label).data('question', q)
        .appendTo($questionDropdown);
    });
    $questionDropdown.prop('disabled', questions.length === 0).show();
  }

  // -------------------------------------------------------------------------
  // Question selected → show value controls
  // -------------------------------------------------------------------------
  function onQuestionChange() {
    var q_id = $questionDropdown.val();
    if (!q_id) {
      resetValueControls();
      return;
    }

    var q = getQuestionById(q_id);
    if (!q) return;

    $valueWrap.show();
    $valueInput.hide().val('');
    $valueSelect.hide().empty();

    if (q.options && q.options.length) {
      // Choice type — show dropdown
      $valueSelect.empty().append(
        $('<option value="">').text('— ' + Drupal.t('Select a value') + ' —')
      );
      $.each(q.options, function (_, opt) {
        $('<option>').val(opt).text(opt).appendTo($valueSelect);
      });
      $valueSelect.show();
    }
    else {
      // Free-text type
      $valueInput.show().focus();
    }

    $addFilterBtn.show();
  }

  // -------------------------------------------------------------------------
  // Add filter chip
  // -------------------------------------------------------------------------
  function onAddFilter() {
    var q_id = $questionDropdown.val();
    if (!q_id) return;

    var q = getQuestionById(q_id);
    if (!q) return;

    var value = '';
    if ($valueSelect.is(':visible')) {
      value = $valueSelect.val();
    }
    else {
      value = $valueInput.val().trim();
    }

    if (value === '' || value === null) return;

    // Prevent duplicate q_id + value combos
    var duplicate = false;
    $.each(state.active_filters, function (_, f) {
      if (f.q_id === q_id && f.value === value) { duplicate = true; }
    });
    if (duplicate) return;

    state.active_filters.push({
      q_id   : q.q_id,
      label  : q.label,
      type   : q.type,
      value  : value,
      display: q.label + ': ' + value,
    });

    renderFilterChips();
    resetValueControls();
    state.page = 0;
    fetchResponses();
  }

  function renderFilterChips() {
    $filterChips.empty();
    $.each(state.active_filters, function (i, f) {
      var $chip = $('<span class="mfs-chip">').text(f.display);
      $('<button class="mfs-chip-remove" aria-label="' + Drupal.t('Remove filter') + '">×</button>')
        .on('click', function () { removeFilter(i); })
        .appendTo($chip);
      $chip.appendTo($filterChips);
    });
  }

  function removeFilter(index) {
    state.active_filters.splice(index, 1);
    renderFilterChips();
    state.page = 0;
    fetchResponses();
  }

  // -------------------------------------------------------------------------
  // Fetch responses
  // -------------------------------------------------------------------------
  function fetchResponses() {
    if (!state.form_label || state.loading) return;
    state.loading = true;

    setStatus(Drupal.t('Searching…'));
    $tableWrap.addClass('mfs-loading');

    var filters = $.map(state.active_filters, function (f) {
      return { label: f.label, value: f.value, type: f.type };
    });

    $.ajax({
      url        : Drupal.settings.modularformSearch.responsesUrl,
      method     : 'POST',
      contentType: 'application/json',
      data       : JSON.stringify({
        form_label: state.form_label,
        filters   : filters,
        page      : state.page,
      }),
    })
      .done(function (data) {
        state.total       = data.total  || 0;
        state.any_editable= data.any_editable || false;
        renderTable(data.rows || [], data);
        renderPager(data.total, data.page, data.limit);
        setStatus(Drupal.t('@count result(s)', { '@count': data.total }));
      })
      .fail(function () {
        setStatus(Drupal.t('Search failed.'), true);
      })
      .always(function () {
        state.loading = false;
        $tableWrap.removeClass('mfs-loading');
      });
  }

  // -------------------------------------------------------------------------
  // Render table
  // -------------------------------------------------------------------------
  function renderTable(rows, meta) {
    var $table = $tableWrap.find('table');
    if (!$table.length) {
      $table = $('<table class="views-table cols-0">').appendTo($tableWrap);
    }

    // Rebuild thead
    var $thead = $table.find('thead');
    if (!$thead.length) $thead = $('<thead>').prependTo($table);
    $thead.empty();
    var $hr = $('<tr>').appendTo($thead);
    $('<th>').text(Drupal.t('Form')).appendTo($hr);
    $('<th>').text(Drupal.t('Email')).appendTo($hr);
    $('<th>').text(Drupal.t('Submitted')).appendTo($hr);
    if (meta.any_editable) {
      $('<th>').text(Drupal.t('Updated')).appendTo($hr);
    }
    $('<th>').text(Drupal.t('Operations')).appendTo($hr);

    // Rebuild tbody
    var $tbody = $table.find('tbody');
    if (!$tbody.length) $tbody = $('<tbody>').appendTo($table);
    $tbody.empty();

    if (!rows.length) {
      var colspan = meta.any_editable ? 5 : 4;
      $('<tr><td colspan="' + colspan + '">' + Drupal.t('No responses found.') + '</td></tr>')
        .appendTo($tbody);
      return;
    }

    $.each(rows, function (_, row) {
      var $tr = $('<tr>').appendTo($tbody);
      $('<td>').text(row.form_title).appendTo($tr);
      $('<td>').text(row.email).appendTo($tr);
      $('<td>').text(row.submitted_formatted).appendTo($tr);
      if (meta.any_editable) {
        $('<td>').text(row.updated_formatted).appendTo($tr);
      }

      // Operations cell
      var base = Drupal.settings.basePath + 'forms/response/';
      var $ops = $('<td class="mfs-ops">');
      $('<a class="mfs-op-link">').attr('href', base + 'view/' + row.response_id)
        .text(Drupal.t('View')).appendTo($ops);
      if (row.editable) {
        $(' | ').appendTo($ops);
        $('<a class="mfs-op-link">').attr('href', base + 'edit/' + row.response_id)
          .text(Drupal.t('Edit')).appendTo($ops);
      }
      $(' | ').appendTo($ops);
      $('<a class="mfs-op-link mfs-op-delete">').attr('href', base + 'delete/' + row.response_id)
        .text(Drupal.t('Delete')).appendTo($ops);
      $ops.appendTo($tr);
    });
  }

  // -------------------------------------------------------------------------
  // Pager
  // -------------------------------------------------------------------------
  function renderPager(total, page, limit) {
    $pagerWrap.empty();
    if (total <= limit) return;

    var pages = Math.ceil(total / limit);
    for (var i = 0; i < pages; i++) {
      var $btn = $('<button class="mfs-pager-btn">').data('page', i).text(i + 1);
      if (i === page) $btn.addClass('is-current');
      $btn.appendTo($pagerWrap);
    }
  }

  // -------------------------------------------------------------------------
  // Helpers
  // -------------------------------------------------------------------------
  function getQuestionById(q_id) {
    var found = null;
    $.each(state.questions, function (_, q) {
      if (q.q_id === q_id) { found = q; return false; }
    });
    return found;
  }

  function resetValueControls() {
    $valueWrap.hide();
    $valueInput.hide().val('');
    $valueSelect.hide().empty();
    $addFilterBtn.hide();
    $questionDropdown.val('');
  }

  function setStatus(msg, isError) {
    $statusBar.text(msg).toggleClass('mfs-status--error', !!isError);
  }

  function debounce(fn, ms) {
    var timer;
    return function () {
      clearTimeout(timer);
      timer = setTimeout(fn, ms);
    };
  }

})(jQuery, Drupal);
