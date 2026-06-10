<div class="gform-wrapper">
  <div class="gform-container">

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
      <h1 class="gform-title"><?php print t('Deleted Forms'); ?></h1>
    </div>

    <div class="gform-section-label"><?php print t('Deleted forms'); ?></div>

    <div class="gform-card">

      <?php if (!empty($rows) && !isset($rows[0]['data'])): ?>
        <div class="gform-table-toolbar">
          <span class="gform-table-count">
            <?php print count($rows); ?><?php print t('forms'); ?>
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
          <?php if (!empty($rows) && !isset($rows[0]['data'])): ?>
            <?php foreach ($rows as $row): ?>
              <tr>
                <?php
                // Index 0: Name
                // Index 1: Owner
                // Index 2: Status
                // Index 3: Created
                // Index 4: Deleted On
                // Index 5: Operations
                ?>
                <td class="gform-td-name">
                  <div class="gform-name-inner">
                    <span class="gform-form-icon gform-form-icon--deleted">
                      <svg viewBox="0 0 13 13" fill="white"
                           xmlns="http://www.w3.org/2000/svg">
                        <rect x="1.5" y="2" width="10" height="1.8" rx=".9"/>
                        <rect x="1.5" y="5.5" width="10" height="1.8" rx=".9"/>
                        <rect x="1.5" y="9" width="7" height="1.8" rx=".9"/>
                      </svg>
                    </span>
                    <span class="gform-name-text"><?php print $row[0]; ?></span>
                  </div>
                </td>
                <td><?php print $row[1]; ?></td>
                <td><?php print $row[2]; ?></td>
                <td><?php print $row[3]; ?></td>
                <td>
                  <span
                    class="gform-deleted-date"><?php print $row[4]; ?></span>
                </td>
                <td class="gform-td-ops">
                  <div class="action-dropdown">
                    <button class="action-dropbtn">Actions &#9660;</button>
                    <div class="action-dropdown-content">
                      <?php
                      if (is_array($row[5]) && !empty($row[5])):
                        foreach ($row[5] as $key => $link):
                          $attributes = isset($link['attributes']) ? $link['attributes'] : [];
                          // Add a danger class to the permanent delete link
                          if ($key === 'permanent_delete') {
                            $attributes['class'][] = 'gform-link--danger';
                          }
                          print l($link['title'], $link['href'], [
                            'attributes' => $attributes,
                            'query' => isset($link['query']) ? $link['query'] : [],
                          ]);
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
              <td colspan="6">
                <div class="gform-empty-state">
                  <svg viewBox="0 0 48 48" fill="none"
                       xmlns="http://www.w3.org/2000/svg" width="48"
                       height="48">
                    <rect x="8" y="6" width="32" height="36" rx="4"
                          fill="#f3e5f5" opacity=".8"/>
                    <rect x="14" y="14" width="20" height="3" rx="1.5"
                          fill="#ce93d8"/>
                    <rect x="14" y="21" width="20" height="3" rx="1.5"
                          fill="#ce93d8"/>
                    <rect x="14" y="28" width="13" height="3" rx="1.5"
                          fill="#ce93d8" opacity=".5"/>
                  </svg>
                  <p><?php print t('No deleted forms found.'); ?></p>
                </div>
              </td>
            </tr>
          <?php endif; ?>
          </tbody>
        </table>
      </div>
    </div>

    <div class="gform-back-link">
      <?php print l('← ' . t('Back to all forms'), 'forms'); ?>
    </div>

  </div>
</div>
