(function ($) {
  'use strict';

  Drupal.behaviors.modularformWordCloud = {
    attach: function (context, settings) {
      var config = settings.modularformWordCloud;
      if (!config || !config.words || !config.words.length) return;

      // Only init once per page.
      var container = document.getElementById('mf-wordcloud');
      if (!container || container.dataset.wcInit) return;
      container.dataset.wcInit = '1';

      renderWordCloud(config.words, 'mf-wordcloud');
    }
  };

  /**
   * Render a word cloud into a container by id.
   *
   * @param {Array}  words       Array of {text, size, count}.
   * @param {string} containerId ID of the target div.
   */
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

  /**
   * Draw placed words into an SVG inside the container.
   */
  function draw(words, containerId, width, height, colors) {
    d3.select('#' + containerId)
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
          .style('cursor',      'default')
          .attr('text-anchor',  'middle')
          .attr('transform',    function (d) {
            return 'translate(' + [d.x, d.y] + ')rotate(' + d.rotate + ')';
          })
          .text(function (d) { return d.text; })
          .append('title')
            .text(function (d) { return d.text + ': ' + d.count + ' uses'; });
  }

}(jQuery));
