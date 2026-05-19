// drag and drop js file
(function ($, Drupal) {
  'use strict';

  Drupal.behaviors.modularformDragDrop = {
    attach: function (context) {
      var $topWrapper = $('#modular-form-wrapper');
      if ($topWrapper.length) {
        enableDrag($topWrapper[0], '.depth-section', 'section', 'section-weight');
      }

      $('.depth-section').each(function () {
        enableDrag(this, '.depth-question', 'question', 'question-weight');
      });

      $('.depth-question').each(function () {
        enableDrag(this, '.depth-option', 'option', 'option-weight');
      });
    }
  };

  Drupal.behaviors.modularformCollapse = {
    attach: function (context) {
      $(context).find('.collapse-toggle').once('collapse-toggle').on('click', function () {
        var targetId = $(this).data('target');
        var $body = $('#' + targetId);
        var expanded = $(this).attr('aria-expanded') === 'true';

        if (expanded) {
          // Before collapsing, grab the current field value and show in header
          var $label = $(this).siblings('.element-header-label');
          var $questionField = $body.find('input[name*="question_text"]').first();
          var $sectionField = $body.find('input[name*="[title]"]').first();

          var currentVal = $questionField.val() || $sectionField.val() || $label.text();
          if (currentVal) {
            $label.text(currentVal);
          }

          $body.slideUp(150);
          $(this).attr('aria-expanded', 'false').text('▸');
        } else {
          $body.slideDown(150);
          $(this).attr('aria-expanded', 'true').text('▾');
        }
      });

      // Keep header label live while typing (even before collapsing)
      $(context).find('.depth-question, .depth-section').once('header-sync').each(function () {
        var $container = $(this);
        var $label = $container.find('.element-header-label').first();

        $container.find('input[name*="question_text"], input[name*="[title]"]')
          .first()
          .on('input', function () {
            $label.text($(this).val() || $label.text());
          });
      });
    }
  };

  function enableDrag(container, itemSel, scope, weightClass) {
    var $container = $(container);

    $container.off('mousedown.dnd-' + scope)
      .on('mousedown.dnd-' + scope, '[data-drag-scope="' + scope + '"]', function (e) {
        e.preventDefault();

        var $item = $(this).closest(itemSel);
        if (!$item.length) return;

        var dragging = $item[0];
        var rect     = dragging.getBoundingClientRect();
        var startY   = e.clientY;
        var startX   = e.clientX;
        var startTop = rect.top;
        var startLeft = rect.left;

        var $placeholder = $('<div class="dnd-placeholder">').css({ height: rect.height });

        dragging.classList.add('dnd-dragging');
        $item.after($placeholder);

        $(dragging).css({
          position:      'fixed',
          zIndex:        9999,
          width:         rect.width,
          left:          rect.left,
          top:           rect.top,
          pointerEvents: 'none',
        });

        function onMove(e) {
          $(dragging).css({
            top:  startTop  + (e.clientY - startY),
            left: startLeft + (e.clientX - startX),
          });

          if (scope === 'question') {
            // Check all sections for a valid drop target
            var inserted = false;
            $('.depth-section').each(function () {
              var secRect = this.getBoundingClientRect();
              if (
                e.clientX >= secRect.left && e.clientX <= secRect.right &&
                e.clientY >= secRect.top  && e.clientY <= secRect.bottom
              ) {
                var $sec = $(this);
                var $siblings = $sec.children('.depth-question').not(dragging);
                var placedInSibling = false;

                $siblings.each(function () {
                  var sibRect = this.getBoundingClientRect();
                  if (e.clientY < sibRect.top + sibRect.height / 2) {
                    $(this).before($placeholder);
                    placedInSibling = true;
                    return false;
                  }
                });

                if (!placedInSibling) {
                  // Append before the add_q button
                  var $addQ = $sec.children('[name^="add_q_"]');
                  $addQ.length ? $addQ.before($placeholder) : $sec.append($placeholder);
                }

                inserted = true;
                return false;
              }
            });

            if (!inserted) {
              // Fallback — keep placeholder in current position
            }

          } else {
            // Sections and options — same container only
            var inserted = false;
            $container.children(itemSel).not(dragging).each(function () {
              var sibRect = this.getBoundingClientRect();
              if (e.clientY < sibRect.top + sibRect.height / 2) {
                $(this).before($placeholder);
                inserted = true;
                return false;
              }
            });
            if (!inserted) {
              $container.children(itemSel).not(dragging).last().after($placeholder);
            }
          }
        }

        function onUp() {
          $(document).off('mousemove.dnd-' + scope + ' mouseup.dnd-' + scope);

          $placeholder.replaceWith(dragging);
          $(dragging).css({ position: '', zIndex: '', width: '', left: '', top: '', pointerEvents: '' });
          dragging.classList.remove('dnd-dragging');

          if (scope === 'question') {
            // Update parent_sec hidden input to whichever section now contains this question
            var $newSection = $(dragging).closest('.depth-section');
            if ($newSection.length) {
              var newSecId = $newSection.attr('id').replace('section-wrapper-', '');
              $(dragging).find('input.question-parent-sec').first().val(newSecId);
            }

            // Update weights within the new section
            $(dragging).closest('.depth-section')
              .children('.depth-question').each(function (i) {
              $(this).find('input.question-weight').first().val(i * 10);
            });

            // Also rewrite weights in the old section (placeholder was there)
            $('.depth-section').each(function () {
              $(this).children('.depth-question').each(function (i) {
                $(this).find('input.question-weight').first().val(i * 10);
              });
            });

          } else {
            $container.children(itemSel).each(function (i) {
              $(this).find('input.' + weightClass).first().val(i * 10);
            });
          }
        }

        $(document)
          .on('mousemove.dnd-' + scope, onMove)
          .on('mouseup.dnd-' + scope, onUp);
      });
  }

})(jQuery, Drupal);
