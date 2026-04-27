<div class="gform-wrapper">
  <div class="gform-container">

    <div class="gform-header">
      <div class="gform-header-icon">
        <svg viewBox="0 0 22 22" fill="none" xmlns="http://www.w3.org/2000/svg">
          <rect x="3" y="2" width="16" height="18" rx="2" fill="white" opacity=".9"/>
          <rect x="6" y="6" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="10" width="10" height="2" rx="1" fill="#673ab7"/>
          <rect x="6" y="14" width="6" height="2" rx="1" fill="#673ab7"/>
        </svg>
      </div>
      <h1 class="gform-title"><?php print t('Recent forms'); ?></h1>
    </div>

    <?php if (!empty($rows)): ?>
      <div class="gform-section-label"><?php print t('Recent'); ?></div>
      <div class="gform-thumb-grid">
<!--        //add new-->
        <?php
          $content = '<div class="gform-thumb-grid">
                          <div class="gform-thumb">
                            <div class="gform-thumb-preview" style="background: ' . $color['bg'] . ';">
                              ' . t('+ New') . '
                            </div>
                            <div class="gform-thumb-meta">
                              <div class="gform-thumb-info">
                                ' . t('+ Create a new form') . '
                              </div>
                            </div>
                          </div>
                        </div>';
            print l($content, 'forms/create', array(
              'html' => TRUE,
              'attributes' => array(
                'class' => array('add-btn')
              ),
            ));
        ?>

        <?php
        $thumb_colors = array(
          array('bg' => '#f3e5f5', 'fill' => '#ce93d8', 'line' => '#6a1b9a'),
          array('bg' => '#e8f5e9', 'fill' => '#a5d6a7', 'line' => '#1b5e20'),
          array('bg' => '#e3f2fd', 'fill' => '#90caf9', 'line' => '#0d47a1'),
          array('bg' => '#fff8e1', 'fill' => '#ffe082', 'line' => '#e65100'),
        );

        $recent = array_slice($rows, 0, 4);
        foreach ($recent as $i => $row):
          $color = $thumb_colors[$i % count($thumb_colors)];
          $name  = isset($row[0]) ? $row[0] : '';
          $date  = isset($row[2]) ? $row[2] : '';
          ?>

          <div class="gform-thumb">
            <div class="gform-thumb-preview" style="background: <?php print $color['bg']; ?>">
              <svg width="44" height="44" viewBox="0 0 44 44" xmlns="http://www.w3.org/2000/svg">
                <rect x="7" y="5" width="30" height="34" rx="3" fill="<?php print $color['fill']; ?>"/>
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
              <button class="gform-thumb-btn" title="<?php print t('More options'); ?>">
                <svg viewBox="0 0 24 24" fill="currentColor" xmlns="http://www.w3.org/2000/svg">
                  <circle cx="12" cy="5"  r="1.8"/>
                  <circle cx="12" cy="12" r="1.8"/>
                  <circle cx="12" cy="19" r="1.8"/>
                </svg>
              </button>
            </div>
          </div>
        <?php endforeach; ?>
      </div>
    <?php else:?>
      <div class="gform-thumb-grid">
          <div class="gform-thumb">
            <div class="gform-thumb-preview" style="background: <?php print $color['bg']; ?>">
              <svg width="44" height="44" viewBox="0 0 44 44" xmlns="http://www.w3.org/2000/svg">
                <rect x="7" y="5" width="30" height="34" rx="3" fill="<?php print $color['fill']; ?>"/>
                <rect x="11" y="11" width="22" height="3" rx="1.5" fill="<?php print $color['line']; ?>"/>
                <rect x="11" y="17" width="22" height="3" rx="1.5" fill="<?php print $color['line']; ?>"/>
                <rect x="11" y="23" width="15" height="3" rx="1.5" fill="<?php print $color['line']; ?>"/>
                <rect x="11" y="29" width="18" height="3" rx="1.5" fill="<?php print $color['line']; ?>" opacity=".5"/>
              </svg>
            </div>
            <div class="gform-thumb-meta">
              <div class="gform-thumb-info">
                +
              </div>
              <button class="gform-thumb-btn" title="<?php print t('More options'); ?>">
                <svg viewBox="0 0 24 24" fill="currentColor" xmlns="http://www.w3.org/2000/svg">
                  <circle cx="12" cy="5"  r="1.8"/>
                  <circle cx="12" cy="12" r="1.8"/>
                  <circle cx="12" cy="19" r="1.8"/>
                </svg>
              </button>
            </div>
          </div>
      </div>
    <?php endif; ?>


    <div class="gform-section-label"><?php print t('All forms'); ?></div>
    <div class="gform-card">

      <?php if (!empty($header)): ?>
        <div class="gform-table-toolbar">
          <span class="gform-table-count">
            <?php print count($rows); ?> <?php print t('forms'); ?>
          </span>
        </div>
      <?php endif; ?>

      <div class="gform-table-wrap">
        <table class="gform-table">

          <?php if (!empty($header)): ?>
            <thead>
            <tr>
              <?php foreach ($header as $cell): ?>
                <th><?php print $cell; ?></th>
              <?php endforeach; ?>
            </tr>
            </thead>
          <?php endif; ?>

          <tbody>
          <?php if (!empty($rows)): ?>
            <?php foreach ($rows as $row): ?>
              <tr>
                <?php $col = 0; ?>
                <?php foreach ($row as $data): ?>
                  <?php if ($col === 0): ?>
                    <td class="gform-td-name">
                      <div class="gform-name-inner">
                          <span class="gform-form-icon">
                            <svg viewBox="0 0 13 13" fill="white" xmlns="http://www.w3.org/2000/svg">
                              <rect x="1.5" y="2"   width="10" height="1.8" rx=".9"/>
                              <rect x="1.5" y="5.5" width="10" height="1.8" rx=".9"/>
                              <rect x="1.5" y="9"   width="7"  height="1.8" rx=".9"/>
                            </svg>
                          </span>
                        <span class="gform-name-text"><?php print check_plain($data); ?></span>
                      </div>
                    </td>
                  <?php elseif ($col === count($row) - 1): ?>
                    <td class="gform-td-ops"><?php print $data; ?></td>
                  <?php else: ?>
                    <td><?php print check_plain($data); ?></td>
                  <?php endif; ?>
                  <?php $col++; ?>
                <?php endforeach; ?>
              </tr>
            <?php endforeach; ?>
          <?php else: ?>
            <tr class="gform-empty-row">
              <td colspan="4"><?php print t('No forms found.'); ?></td>
            </tr>
          <?php endif; ?>
          </tbody>

        </table>
      </div>
    </div>

  </div>
</div>
