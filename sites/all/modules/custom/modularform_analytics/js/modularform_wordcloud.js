(function ($) {
  'use strict';

  Drupal.behaviors.modularformWordCloud = {
    attach: function (context, settings) {
      var config = settings.modularformWordCloud;
      if (!config || !config.words || !config.words.length) return;

      var container = document.getElementById('mf-wordcloud');
      if (!container || container.dataset.wcInit) return;
      container.dataset.wcInit = '1';

      if (!$('#mf-wc-hover').length) {
        $('<div id="mf-wc-hover">').css({
          position:      'fixed',
          pointerEvents: 'auto',
          zIndex:        9999,
          background:    '#fff',
          border:        '1px solid #ddd',
          borderRadius:  '4px',
          boxShadow:     '0 2px 8px rgba(0,0,0,.15)',
          padding:       '10px 14px',
          fontSize:      '13px',
          fontFamily:    'sans-serif',
          lineHeight:    '1.6',
          maxWidth:      '260px',
          display:       'none',
        }).appendTo('body');
      }

      renderWordCloud(config.words, 'mf-wordcloud');
    }
  };

  function renderWordCloud(words, containerId) {
    var el     = document.getElementById(containerId);
    var width  = el.offsetWidth  || 800;
    var height = el.offsetHeight || 450;
    var colors = [
      '#1565c0', '#283593', '#0277bd', '#00838f',
      '#2e7d32', '#558b2f', '#6a1520', '#4527a0',
      '#ad1457', '#00695c'
    ];

    d3.layout.cloud()
      .size([width, height])
      .words(words)
      .padding(6)
      .rotate(function () { return ~~(Math.random() * 2) * 90; })
      .font('Impact, sans-serif')
      .fontSize(function (d) { return d.size; })
      .on('end', function (placed) {
        draw(placed, containerId, width, height, colors);
      })
      .start();
  }

  function draw(words, containerId, width, height, colors) {
    var $hover = $('#mf-wc-hover');

    var texts = d3.select('#' + containerId)
      .append('svg')
      .attr('width', width)
      .attr('height', height)
      .append('g')
      .attr('transform', 'translate(' + width / 2 + ',' + height / 2 + ')')
      .selectAll('text')
      .data(words)
      .enter()
      .append('text')
      .style('font-size',   function (d) { return d.size + 'px'; })
      .style('font-family', 'Impact, sans-serif')
      .style('fill',        function (d, i) { return colors[i % colors.length]; })
      .style('cursor',      'pointer')
      .attr('text-anchor',  'middle')
      .attr('transform',    function (d) {
        return 'translate(' + [d.x, d.y] + ')rotate(' + d.rotate + ')';
      })
      .text(function (d) { return d.text; });

    // ── Hover ──────────────────────────────────────────────────────────────

    texts
      .on('mouseover', function (d) {
        var sources = d.sources || [];
        var maxCnt  = sources.reduce(function (m, s) { return Math.max(m, s.count); }, 1);

        var byForm = {};
        var formOrder = [];
        sources.forEach(function (s) {
          if (!byForm[s.form_id]) {
            byForm[s.form_id] = { form_title: s.form_title, rows: [] };
            formOrder.push(s.form_id);
          }
          byForm[s.form_id].rows.push(s);
        });

        var html = '<strong style="font-size:14px">' + escHtml(d.text) + '</strong>' +
          ' <span style="color:#888;font-size:12px">(' + d.count + ' total)</span>';

        formOrder.forEach(function (fid, fi) {
          var group = byForm[fid];
          html += '<div style="margin-top:8px;font-size:12px;font-weight:600;color:#444">' +
            escHtml(group.form_title) +
            '</div>';

          group.rows.forEach(function (s, si) {
            var pct   = Math.round((s.count / maxCnt) * 100);
            var color = colors[(fi + si) % colors.length];
            var url   = Drupal.settings.basePath + 'forms/response/list?' + $.param({
              form_id:                s.form_id,
              'filters[0][solr_key]': s.q_solr_key,
              'filters[0][answer]':   d.text,
            });
            html +=
              '<div class="mf-wc-source-row" data-url="' + escHtml(url) + '" ' +
              'style="margin:3px 0;cursor:pointer;padding:2px 4px;border-radius:3px">' +
              '<div style="font-size:11px;color:#555;margin-bottom:2px">' + escHtml(s.q_label) + '</div>' +
              '<div style="display:flex;align-items:center;gap:6px">' +
              '<div style="flex:1;background:#eee;border-radius:3px;height:8px">' +
              '<div style="width:' + pct + '%;background:' + color + ';height:8px;border-radius:3px"></div>' +
              '</div>' +
              '<span style="font-size:11px;color:#555;flex:0 0 24px;text-align:right">' + s.count + '</span>' +
              '</div>' +
              '</div>';
          });
        });

        $hover.html(html).show();
      })
      .on('mousemove', function () {
        $hover.css({ left: (event.clientX + 14) + 'px', top: (event.clientY - 10) + 'px' });
      })
      .on('mouseout', function () {
        $hover.hide();
      });

    // ── Click rows inside hover panel ──────────────────────────────────────

    $('#mf-wc-hover').on('click', '.mf-wc-source-row', function () {
      window.location.href = $(this).data('url');
    });
    $('#mf-wc-hover')
      .on('mouseenter', function () { $(this).show(); })
      .on('mouseleave', function () { $(this).hide(); });
  }

  function escHtml(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

}(jQuery));
