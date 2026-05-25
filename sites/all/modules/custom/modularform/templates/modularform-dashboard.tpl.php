<?php
/**
 * @file modularform-dashboard.tpl.php
 * Template for the Modular Form dashboard.
 *
 * Available variables:
 *   $header  — array of column header labels
 *   $rows    — array of form rows for the initial server-rendered table
 *              each row: [title, owner, status, created, links[]]
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

    <!-- ══════════════════════════════
         HEADER
    ══════════════════════════════ -->
    <div class="gform-header">
      <div class="gform-header-icon">
        <svg viewBox="0 0 22 22" fill="none" xmlns="http://www.w3.org/2000/svg">
          <rect x="3" y="2" width="16" height="18" rx="2" fill="white" opacity=".9"/>
          <rect x="6" y="6"  width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="10" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="14" width="6"  height="2" rx="1" fill="#673ab7"/>
        </svg>
      </div>
      <h1 class="gform-title"><?php print t('Forms'); ?></h1>
    </div>

    <!-- ══════════════════════════════
         RECENT THUMBNAILS
    ══════════════════════════════ -->
    <div class="gform-section-label"><?php print t('Recent'); ?></div>
    <div class="gform-thumb-grid">

      <?php
      /* ── "Create new" tile ── */
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
      print l($new_content, 'forms/create', ['html' => TRUE, 'attributes' => ['class' => ['gform-thumb-link']]]);
      ?>

      <?php foreach ($recent as $i => $row):
        $color = $thumb_colors[$i % count($thumb_colors)];
        $name  = isset($row[0]) ? $row[0] : '';
        $date  = isset($row[3]) ? $row[3] : '';
        $links = isset($row[4]) ? $row[4] : [];
        $edit_href = isset($links['builder_edit']['href']) ? $links['builder_edit']['href'] : '#';
        ?>
        <a href="<?php print check_url($edit_href); ?>" class="gform-thumb-link">
          <div class="gform-thumb">
            <div class="gform-thumb-preview" style="background:<?php print $color['bg']; ?>;">
              <svg width="44" height="44" viewBox="0 0 44 44" xmlns="http://www.w3.org/2000/svg">
                <rect x="7"  y="5"  width="30" height="34" rx="3" fill="<?php print $color['fill']; ?>"/>
                <rect x="11" y="11" width="22" height="3" rx="1.5" fill="<?php print $color['line']; ?>"/>
                <rect x="11" y="17" width="22" height="3" rx="1.5" fill="<?php print $color['line']; ?>"/>
                <rect x="11" y="23" width="15" height="3" rx="1.5" fill="<?php print $color['line']; ?>"/>
                <rect x="11" y="29" width="18" height="3" rx="1.5" fill="<?php print $color['line']; ?>" opacity=".5"/>
              </svg>
            </div>
            <div class="gform-thumb-meta">
              <div class="gform-thumb-info">
                <div class="gform-thumb-name"><?php print check_plain($name); ?></div>
                <?php if ($date): ?>
                  <div class="gform-thumb-date"><?php print check_plain($date); ?></div>
                <?php endif; ?>
              </div>
            </div>
          </div>
        </a>
      <?php endforeach; ?>

    </div><!-- /gform-thumb-grid -->

    <!-- ══════════════════════════════
         SEARCH ROW  (row 1)
    ══════════════════════════════ -->
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

    <!-- ══════════════════════════════
         FILTER ROW  (row 2)
    ══════════════════════════════ -->
    <div id="mf-filters">

      <select id="mf-filter-status" aria-label="<?php print t('Filter by status'); ?>">
        <option value=""><?php print t('All statuses'); ?></option>
        <option value="1"><?php print t('Published'); ?></option>
        <option value="0"><?php print t('Draft'); ?></option>
      </select>

      <select id="mf-filter-visibility" aria-label="<?php print t('Filter by visibility'); ?>">
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
        <input type="checkbox" id="mf-filter-owner" />
        <?php print t('My forms only'); ?>
      </label>

    </div><!-- /mf-filters -->

    <!-- ══════════════════════════════
         ALL FORMS TABLE
    ══════════════════════════════ -->
    <div class="gform-section-label"><?php print t('All forms'); ?></div>

    <div class="gform-card">

      <div class="gform-table-toolbar">
        <span id="mf-meta" aria-live="polite">
          <?php print count($rows) . ' ' . t('forms'); ?>
        </span>
      </div>

      <div class="gform-table-wrap" id="mf-table-wrap">
        <table class="gform-table" id="mf-table" aria-label="<?php print t('Forms list'); ?>">
          <thead>
          <tr>
            <?php foreach ($header as $cell): ?>
              <th><?php print $cell; ?></th>
            <?php endforeach; ?>
          </tr>
          </thead>

          <tbody id="mf-form-rows">
          <?php if (!empty($rows)): ?>
            <?php foreach ($rows as $row): ?>
              <tr>
                <!-- Name -->
                <td class="gform-td-name">
                  <div class="gform-name-inner">
                      <span class="gform-form-icon">
                        <svg viewBox="0 0 13 13" fill="white" xmlns="http://www.w3.org/2000/svg">
                          <rect x="1.5" y="2"   width="10" height="1.8" rx=".9"/>
                          <rect x="1.5" y="5.5" width="10" height="1.8" rx=".9"/>
                          <rect x="1.5" y="9"   width="7"  height="1.8" rx=".9"/>
                        </svg>
                      </span>
                    <span class="gform-name-text"><?php print $row[0]; ?></span>
                  </div>
                </td>
                <!-- Owner -->
                <td><?php print $row[1]; ?></td>
                <!-- Status -->
                <td>
                  <?php
                  $status_val = ($row[2] === t('Published')) ? 'published' : 'draft';
                  ?>
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
                      <?php if (is_array($row[4])): ?>
                        <?php foreach ($row[4] as $link): ?>
                          <?php
                          $attrs = isset($link['attributes']) ? $link['attributes'] : [];
                          print l($link['title'], $link['href'], ['attributes' => $attrs]);
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
              <td colspan="5"><?php print t('No forms found.'); ?></td>
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
