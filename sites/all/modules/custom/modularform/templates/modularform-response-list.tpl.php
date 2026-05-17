<div class="gform-wrapper">
  <div class="gform-container">

    <div class="gform-header">
      <div class="gform-header-icon">
        <svg viewBox="0 0 22 22" fill="none" xmlns="http://www.w3.org/2000/svg">
          <rect x="3" y="2" width="16" height="18" rx="2" fill="white" opacity=".9"/>
          <rect x="6" y="6" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="10" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="14" width="6"  height="2" rx="1" fill="#673ab7" opacity=".5"/>
        </svg>
      </div>
      <h1 class="gform-title"><?php print t('Responses'); ?></h1>
    </div>

    <?php // ── Filter bar ──────────────────────────────────────────────────── ?>
    <div class="gform-filter-bar" id="response-filter-bar">

      <?php // Active filter tags (form tag + pair tags, JS-managed) ?>
      <div class="gform-filter-tags" id="filter-tags"></div>

      <?php // Step 1: form search — always visible ?>
      <div class="gform-filter-step" id="filter-step-form">
        <div class="gform-filter-step__head">
          <span class="gform-filter-step__num">1</span>
          <span class="gform-filter-step__title"><?php print t('Form'); ?></span>
        </div>
        <input type="text" id="filter-form-search"
               placeholder="<?php print t('Search a form…'); ?>"
               autocomplete="off" />
        <div class="gform-filter-panel" id="filter-form-panel" hidden>
          <ul class="gform-filter-list" id="filter-form-list"></ul>
        </div>
        <?php // Shows the chosen form with a "Change" link; hidden until form is picked ?>
        <div class="gform-filter-chosen" id="filter-form-chosen" hidden></div>
      </div>

      <?php // Step 2: question — hidden until form is locked ?>
      <div class="gform-filter-step" id="filter-step-question" hidden>
        <div class="gform-filter-step__head">
          <span class="gform-filter-step__num">2</span>
          <span class="gform-filter-step__title"><?php print t('Question'); ?></span>
        </div>
        <input type="text" id="filter-question-search"
               placeholder="<?php print t('Search questions…'); ?>"
               autocomplete="off" />
        <div class="gform-filter-panel" id="filter-question-panel" hidden>
          <ul class="gform-filter-list" id="filter-question-list"></ul>
        </div>
        <div class="gform-filter-chosen" id="filter-question-chosen" hidden></div>
      </div>

      <?php // Step 3: answer — hidden until question is locked ?>
      <div class="gform-filter-step" id="filter-step-answer" hidden>
        <div class="gform-filter-step__head">
          <span class="gform-filter-step__num">3</span>
          <span class="gform-filter-step__title"><?php print t('Option'); ?></span>
        </div>
        <input type="text" id="filter-answer-search"
               placeholder="<?php print t('Search answers…'); ?>"
               autocomplete="off" />
        <div class="gform-filter-panel" id="filter-answer-panel" hidden>
          <ul class="gform-filter-list" id="filter-answer-list"></ul>
        </div>
        <div class="gform-filter-chosen" id="filter-answer-chosen" hidden></div>
      </div>

      <?php // Commit button — disabled until all three are chosen ?>
      <button class="gform-filter-add" id="filter-add-btn" disabled>
        <?php print t('Add filter'); ?>
      </button>

    </div>

    <div class="gform-section-label">
      <?php print t('Responses'); ?>
    </div>

    <div class="gform-card">

      <?php if (!empty($rows)): ?>
        <div class="gform-table-toolbar">
          <span class="gform-table-count">
            <?php print count($rows); ?> <?php print t('responses'); ?>
          </span>
        </div>
      <?php endif; ?>

      <div class="gform-table-wrap">
        <table class="gform-table">
          <thead>
          <tr>
            <?php foreach ($header as $cell): ?>
              <th><?php print $cell; ?></th>
            <?php endforeach; ?>
          </tr>
          </thead>
          <tbody>
          <?php if (!empty($rows)): ?>
            <?php foreach ($rows as $row): ?>
              <?php
              $ops  = end($row);
              $cols = array_slice($row, 0, -1);
              $is_anon = ($cols[0] === t('Anonymous response'));
              ?>
              <tr>
                <td class="gform-td-name">
                  <div class="gform-name-inner">
                      <span class="gform-form-icon <?php print $is_anon ? 'gform-form-icon--anon' : ''; ?>">
                        <svg viewBox="0 0 13 13" fill="white" xmlns="http://www.w3.org/2000/svg">
                          <?php if ($is_anon): ?>
                            <circle cx="6.5" cy="4" r="2.2"/>
                            <path d="M1.5 11.5c0-2.8 2.2-4.5 5-4.5s5 1.7 5 4.5" stroke="white" stroke-width="1.4" fill="none"/>
                          <?php else: ?>
                            <rect x="1.5" y="3" width="10" height="7" rx="1"/>
                            <path d="M1.5 3.5l5 4 5-4" stroke="#673ab7" stroke-width="1" fill="none"/>
                          <?php endif; ?>
                        </svg>
                      </span>
                    <span class="gform-name-text <?php print $is_anon ? 'gform-name-text--muted' : ''; ?>">
                        <?php print $cols[0]; ?>
                      </span>
                  </div>
                </td>

                <?php foreach (array_slice($cols, 1) as $col): ?>
                  <td><?php print $col; ?></td>
                <?php endforeach; ?>

                <td class="gform-td-ops">
                  <div class="action-dropdown">
                    <button class="action-dropbtn"><?php print t('Actions'); ?> &#9660;</button>
                    <div class="action-dropdown-content">
                      <?php
                      if (is_array($ops) && !empty($ops)):
                        foreach ($ops as $key => $link):
                          $attributes = isset($link['attributes']) ? $link['attributes'] : [];
                          if ($key === 'delete') { $attributes['class'][] = 'gform-link--danger'; }
                          print l($link['title'], $link['href'], ['attributes' => $attributes]);
                        endforeach;
                      endif;
                      ?>
                    </div>
                  </div>
                </td>
              </tr>
            <?php endforeach; ?>

          <?php else: ?>
            <tr class="gform-empty-row">
              <td colspan="<?php print count($header); ?>">
                <div class="gform-empty-state">
                  <svg viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg" width="48" height="48">
                    <rect x="8" y="6" width="32" height="36" rx="4" fill="#f3e5f5" opacity=".8"/>
                    <rect x="14" y="14" width="20" height="3" rx="1.5" fill="#ce93d8"/>
                    <rect x="14" y="21" width="20" height="3" rx="1.5" fill="#ce93d8"/>
                    <rect x="14" y="28" width="13" height="3" rx="1.5" fill="#ce93d8" opacity=".5"/>
                  </svg>
                  <p><?php print t('No responses yet.'); ?></p>
                </div>
              </td>
            </tr>
          <?php endif; ?>
          </tbody>
        </table>
      </div>
    </div>

    <div class="gform-back-link">
      <?php print theme('pager'); ?>
      <?php print l('← ' . t('Back to all forms'), 'forms'); ?>
    </div>

  </div>
</div>
