<?php
/**
 * @file modularform-dashboard.tpl.php
 * Template for the Modular Form dashboard.
 *
 * Available variables:
 *   $header  — array of column header labels
 *   $rows    — array of form rows for the initial server-rendered table
 *              each row: [title, owner, status, created, response_count,
 *   links[], id]
 *
 * The search input and filters trigger AJAX (dashboard.js) which
 * rewrites #mf-form-rows without a page reload.
 */

$thumb_colors = [
  ['bg' => '#f3e5f5', 'fill' => '#ce93d8', 'line' => '#6a1b9a'],
  ['bg' => '#e8f5e9', 'fill' => '#a5d6a7', 'line' => '#1b5e20'],
  ['bg' => '#e3f2fd', 'fill' => '#90caf9', 'line' => '#0d47a1'],
  ['bg' => '#fff8e1', 'fill' => '#ffe082', 'line' => '#e65100'],
];
$recent = array_slice($rows, 0, 4);
?>

<div class="gform-wrapper">
  <div class="gform-container">

    <!-- ════════════════════════════
         HEADER
    ════════════════════════════ -->
    <div class="gform-header">
      <div class="gform-header-icon">
        <svg viewBox="0 0 22 22" fill="none" xmlns="http://www.w3.org/2000/svg">
          <rect x="3" y="2" width="16" height="18" rx="2" fill="white"
                opacity=".9"/>
          <rect x="6" y="6" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="10" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="14" width="6" height="2" rx="1" fill="#673ab7"/>
        </svg>
      </div>
      <h1 class="gform-title"><?php print t('Forms'); ?></h1>
    </div>

    <!-- ════════════════════════════
         RECENT THUMBNAILS
    ════════════════════════════ -->
    <div class="gform-section-label"><?php print t('Recent'); ?></div>
    <div class="gform-thumb-grid">

      <?php
      /* — "Create new" tile — */
      $new_color = $thumb_colors[0];
      $new_content =
        '<div class="gform-thumb">' .
        '<div class="gform-thumb-preview gform-thumb-preview--new" style="background:' . $new_color['bg'] . ';">' .
        '<span class="gform-thumb-new-icon">+</span>' .
        '</div>' .
        '<div class="gform-thumb-meta">' .
        '<div class="gform-thumb-info">' .
        '<div class="gform-thumb-name">' . t('Create a new form') . '</div>' .
        '</div>' .
        '</div>' .
        '</div>';
      print l($new_content, 'forms/create', [
        'html' => TRUE,
        'attributes' => ['class' => ['gform-thumb-link']],
      ]);
      ?>
      <?php foreach (array_slice($recent, 0, 3) as $i => $row):
        $color = $thumb_colors[$i % count($thumb_colors)];
        $name = isset($row[0]) ? $row[0] : '';
        $date = isset($row[3]) ? $row[3] : '';
        $links = isset($row[5]) ? $row[5] : [];
        $view_href = isset($links['view']['href'])
          ? url($links['view']['href'], !empty($links['view']['query']) ? ['query' => $links['view']['query']] : [])
          : '#';
        ?>
        <a href="<?php print check_url($view_href); ?>" class="gform-thumb-link">
          <div class="gform-thumb">
          <div class="gform-thumb-preview"
               style="background:<?php print $color['bg']; ?>;">
            <svg width="44" height="44" viewBox="0 0 44 44"
                 xmlns="http://www.w3.org/2000/svg">
              <rect x="7" y="5" width="30" height="34" rx="3"
                    fill="<?php print $color['fill']; ?>"/>
              <rect x="11" y="11" width="22" height="3" rx="1.5"
                    fill="<?php print $color['line']; ?>"/>
              <rect x="11" y="17" width="22" height="3" rx="1.5"
                    fill="<?php print $color['line']; ?>"/>
              <rect x="11" y="23" width="15" height="3" rx="1.5"
                    fill="<?php print $color['line']; ?>"/>
              <rect x="11" y="29" width="18" height="3" rx="1.5"
                    fill="<?php print $color['line']; ?>" opacity=".5"/>
            </svg>
          </div>
          <div class="gform-thumb-meta">
            <div class="gform-thumb-info">
              <div
                class="gform-thumb-name"><?php print check_plain($name); ?></div>
              <?php if ($date): ?>
                <div
                  class="gform-thumb-date"><?php print check_plain($date); ?></div>
              <?php endif; ?>
            </div>
          </div>
        </div>
        </a>
      <?php endforeach; ?>

    </div><!-- /gform-thumb-grid -->

    <!-- ════════════════════════════
         SEARCH ROW
    ════════════════════════════ -->
    <div id="mf-search-row">
      <div id="mf-search-wrap">
        <i class="ti ti-search" aria-hidden="true"></i>
        <input
          id="mf-search"
          type="search"
          placeholder="<?php print t('Search by name or description…'); ?>"
          autocomplete="off"
          aria-label="<?php print t('Search forms'); ?>"
        />
        <span id="mf-spinner" aria-hidden="true"></span>
      </div>
    </div>

    <!-- ════════════════════════════
         FILTER ROW
    ════════════════════════════ -->
    <div id="mf-filters">

      <select id="mf-filter-status"
              aria-label="<?php print t('Filter by status'); ?>">
        <option value=""><?php print t('All statuses'); ?></option>
        <option value="1"><?php print t('Published'); ?></option>
        <option value="0"><?php print t('Draft'); ?></option>
      </select>

      <select id="mf-filter-visibility"
              aria-label="<?php print t('Filter by visibility'); ?>">
        <option value=""><?php print t('All visibility'); ?></option>
        <option value="0"><?php print t('Public'); ?></option>
        <option value="1"><?php print t('Restricted'); ?></option>
        <option value="2"><?php print t('Private'); ?></option>
      </select>

      <select id="mf-filter-sort" aria-label="<?php print t('Sort by'); ?>">
        <option value="created_desc"><?php print t('Newest first'); ?></option>
        <option value="created_asc"><?php print t('Oldest first'); ?></option>
        <option value="title_asc"><?php print t('Name A–Z'); ?></option>
        <option value="title_desc"><?php print t('Name Z–A'); ?></option>
      </select>

      <label id="mf-owner-label">
        <input type="checkbox" id="mf-filter-owner"/>
        <?php print t('My forms only'); ?>
      </label>

    </div><!-- /mf-filters -->

    <!-- ════════════════════════════
         ALL FORMS TABLE
    ════════════════════════════ -->
    <div class="gform-section-label"><?php print t('All forms'); ?></div>

    <div class="gform-card">

      <div class="gform-table-toolbar">
        <span id="mf-meta" aria-live="polite">
          <?php print count($rows) . ' ' . t('forms'); ?>
        </span>
        <!-- Export toggle button -->
        <button id="mf-export-toggle" class="mf-export-toggle-btn"
                aria-pressed="false"
                title="<?php print t('Select forms to export'); ?>">
          <svg viewBox="0 0 16 16" fill="none"
               xmlns="http://www.w3.org/2000/svg"
               aria-hidden="true" width="14" height="14">
            <path d="M8 1v8M5 6l3 3 3-3" stroke="currentColor"
                  stroke-width="1.5"
                  stroke-linecap="round" stroke-linejoin="round"/>
            <path d="M2 11v2a1 1 0 0 0 1 1h10a1 1 0 0 0 1-1v-2"
                  stroke="currentColor" stroke-width="1.5"
                  stroke-linecap="round"/>
          </svg>
          <?php print t('Export'); ?>
        </button>
      </div>

      <div class="gform-table-wrap" id="mf-table-wrap">
        <table class="gform-table" id="mf-table"
               aria-label="<?php print t('Forms list'); ?>">
          <thead>
          <tr>
            <!-- Checkbox column header — hidden until selecting mode -->
            <th class="gform-th-check" aria-hidden="true">
              <input type="checkbox" id="mf-check-all"
                     aria-label="<?php print t('Select all forms'); ?>"
                     tabindex="-1"/>
            </th>
            <?php foreach ($header as $cell): ?>
              <th><?php print $cell; ?></th>
            <?php endforeach; ?>
          </tr>
          </thead>

          <tbody id="mf-form-rows">
          <?php if (!empty($rows)): ?>
            <?php foreach ($rows as $row): ?>
              <tr data-fid="<?php print (int) $row[6]; ?>">
                <!-- Checkbox cell — hidden until selecting mode -->
                <td class="gform-td-check" aria-hidden="true">
                  <input type="checkbox" class="mf-row-check"
                         value="<?php print (int) $row[6]; ?>"
                         aria-label="<?php print t('Select') . ' ' . check_plain($row[0]); ?>"
                         tabindex="-1"/>
                </td>
                <!-- Name -->
                <td class="gform-td-name">
                  <?php
                  $name_link = isset($row[5]['view'])
                    ? $row[5]['view']
                    : (isset($row[5]['name']) ? $row[5]['name'] : NULL);

                  $name_options = [];
                  if ($name_link && !empty($name_link['query'])) {
                    $name_options['query'] = $name_link['query'];
                  }
                  $name_url = $name_link ? url($name_link['href'], $name_options) : '#';
                  ?>
                  <a href="<?php print $name_url; ?>" class="gform-name-link">
                    <div class="gform-name-inner">
                      <span class="gform-form-icon">
                        <svg viewBox="0 0 13 13" fill="white"
                             xmlns="http://www.w3.org/2000/svg">
                          <rect x="1.5" y="2" width="10" height="1.8" rx=".9"/>
                          <rect x="1.5" y="5.5" width="10" height="1.8"
                                rx=".9"/>
                          <rect x="1.5" y="9" width="7" height="1.8" rx=".9"/>
                        </svg>
                      </span>
                      <span
                        class="gform-name-text"><?php print $row[0]; ?></span>
                    </div>
                  </a>
                </td>
                <!-- Responses -->
                <td class="gform-td-responses">
                  <?php $resp_count = (int) $row[4]; ?>
                  <a
                    href="<?php print url('forms/response/list', ['query' => ['form_id' => $row[6]]]); ?>"
                    class="mf-resp-pill<?php print $resp_count === 0 ? ' mf-resp-pill--empty' : ''; ?>"
                    title="<?php print t('View all responses'); ?>">
                    <svg class="mf-resp-pill__icon" viewBox="0 0 14 14"
                         fill="none" xmlns="http://www.w3.org/2000/svg"
                         aria-hidden="true">
                      <path
                        d="M7 1.5C3.96 1.5 1.5 3.69 1.5 6.4c0 1.18.46 2.26 1.22 3.1L2 12.5l2.9-1.1A6.1 6.1 0 0 0 7 11.3c3.04 0 5.5-2.19 5.5-4.9S10.04 1.5 7 1.5Z"
                        fill="currentColor" opacity=".18"/>
                      <path
                        d="M7 1.5C3.96 1.5 1.5 3.69 1.5 6.4c0 1.18.46 2.26 1.22 3.1L2 12.5l2.9-1.1A6.1 6.1 0 0 0 7 11.3c3.04 0 5.5-2.19 5.5-4.9S10.04 1.5 7 1.5Z"
                        stroke="currentColor" stroke-width="1.1"
                        stroke-linejoin="round"/>
                      <rect x="4.5" y="5.5" width="5" height="1" rx=".5"
                            fill="currentColor"/>
                      <rect x="4.5" y="7.5" width="3" height="1" rx=".5"
                            fill="currentColor"/>
                    </svg>
                    <span
                      class="mf-resp-pill__count"><?php print $resp_count; ?></span>
                    <span
                      class="mf-resp-pill__label"><?php print $resp_count === 1 ? t('response') : t('responses'); ?></span>
                  </a>
                </td>
                <!-- Owner -->
                <td><?php print $row[1]; ?></td>
                <!-- Status -->
                <td>
                  <?php $status_val = ($row[2] === t('Published')) ? 'published' : 'draft'; ?>
                  <span class="mf-badge mf-badge--<?php print $status_val; ?>">
                    <?php print $row[2]; ?>
                  </span>
                </td>
                <!-- Created -->
                <td><?php print $row[3]; ?></td>
                <!-- Operations -->
                <td class="gform-td-ops">
                  <div class="action-dropdown">
                    <button class="action-dropbtn">
                      <?php print t('Actions'); ?> &#9660;
                    </button>
                    <div class="action-dropdown-content">
                      <?php if (is_array($row[5])): ?>
                        <?php foreach ($row[5] as $link): ?>
                          <?php
                          $attrs = isset($link['attributes']) ? $link['attributes'] : [];
                          $options = ['attributes' => $attrs];
                          if (!empty($link['query'])) {
                            $options['query'] = $link['query'];
                          }
                          print l($link['title'], $link['href'], $options);
                          ?>
                        <?php endforeach; ?>
                      <?php endif; ?>
                    </div>
                  </div>
                </td>
              </tr>
            <?php endforeach; ?>
          <?php else: ?>
            <tr class="gform-empty-row">
              <td colspan="7"><?php print t('No forms found.'); ?></td>
            </tr>
          <?php endif; ?>
          </tbody>
        </table>
      </div><!-- /gform-table-wrap -->

      <!-- Empty state shown by JS when AJAX returns 0 rows -->
      <div id="mf-empty" hidden>
        <i class="ti ti-file-off" aria-hidden="true"></i>
        <p><?php print t('No forms match your search.'); ?></p>
      </div>
    </div><!-- /gform-card -->

    <!-- Drupal pager (server-side, initial load only) -->
    <?php if ($pager): ?>
      <div id="mf-pager-drupal" class="gform-pager">
        <?php print $pager; ?>
      </div>
    <?php endif; ?>

    <!-- AJAX pager — JS renders into this -->
    <div id="mf-pager" class="gform-pager" style="display:none;"></div>

  </div><!-- /gform-container -->
</div><!-- /gform-wrapper -->

<!-- ════════════════════════════
     EXPORT ACTION BAR
     Slides up from the bottom when ≥1 form is checked
════════════════════════════ -->
<div id="mf-export-bar" aria-live="polite" aria-atomic="true">
  <span id="mf-export-bar-count"></span>
  <div class="mf-export-bar-actions">
    <button id="mf-export-bar-cancel"
            class="mf-export-bar-btn mf-export-bar-btn--ghost">
      <?php print t('Cancel'); ?>
    </button>
    <a id="mf-export-bar-go" href="#"
       class="mf-export-bar-btn mf-export-bar-btn--primary">
      <?php print t('Review Export'); ?>
    </a>
  </div>
</div>
