<?php
/**
 * @file modularform-view-page.tpl.php
 *
 * Variables: see modularform_view_page() in modularform.view.inc
 */

?>
<div class="mfv-page">

  <!-- ===== TOP BAR ===== -->
  <div class="mfv-topbar">
    <div class="mfv-topbar-left">
      <h1 class="mfv-title"><?php print check_plain($form['title']); ?></h1>
      <div class="mfv-meta">
       <span
         class="mfv-badge <?php print $display['status_classes'][(int) $form['status']]; ?>">
          <i class="ti ti-circle-filled" aria-hidden="true"></i>
          <?php print $display['status_labels'][(int) $form['status']]; ?>
       </span>

        <span class="mfv-badge mfv-badge-gray">
          <i
            class="ti <?php print $display['visibility_icons'][(int) $form['visibility']]; ?>"
            aria-hidden="true"></i>
          <?php print $display['visibility_labels'][(int) $form['visibility']]; ?>
        </span>

        <?php if ($display['closed']): ?>
          <span class="mfv-badge mfv-badge-red">
          <i class="ti ti-lock" aria-hidden="true"></i> Closed
        </span>
        <?php else: ?>
          <span class="mfv-badge mfv-badge-green">
          <i class="ti ti-lock-open" aria-hidden="true"></i> Open
        </span>
        <?php endif; ?>

        <?php if (!empty($form['slug'])): ?>
          <span
            class="mfv-meta-slug">slug: <code><?php print check_plain($form['slug']); ?></code></span>
        <?php endif; ?>

        <?php if ($form['closes'] && $form['closes_at']): ?>
          <span class="mfv-meta-text">
            <i class="ti ti-clock"
               aria-hidden="true"></i> Closes <?php print $display['closes_date']; ?>
          </span>
        <?php endif; ?>
      </div>
    </div>
    <div class="mfv-topbar-actions">
      <a href="<?php print $urls['preview']; ?>" class="mfv-btn mfv-btn-sm"
         target="_blank">
        <i class="ti ti-external-link" aria-hidden="true"></i> Preview
      </a>
      <div class="mfv-dropdown-wrap" id="mfv-actions-wrap">
        <button class="mfv-btn mfv-btn-sm" id="mfv-actions-toggle"
                aria-haspopup="true" aria-expanded="false">
          <i class="ti ti-dots-vertical" aria-hidden="true"></i> Actions
          <i class="ti ti-chevron-down" aria-hidden="true"></i>
        </button>
        <div class="mfv-dropdown-menu" id="mfv-actions-menu" role="menu" hidden>
          <a href="<?php print $urls['edit']; ?>" role="menuitem">
            <i class="ti ti-edit" aria-hidden="true"></i> Edit form</a>
          <a href="<?php print $urls['settings']; ?>" role="menuitem">
            <i class="ti ti-settings" aria-hidden="true"></i> Settings</a>
          <a href="<?php print $urls['members']; ?>" role="menuitem">
            <i class="ti ti-users" aria-hidden="true"></i> Manage members</a>
          <a href="<?php print $urls['invites']; ?>" role="menuitem">
            <i class="ti ti-mail" aria-hidden="true"></i> Send invites</a>
          <a href="<?php print $urls['export']; ?>" role="menuitem">
            <i class="ti ti-download" aria-hidden="true"></i> Export
            responses</a>
          <a href="<?php print $urls['delete']; ?>" role="menuitem"
             class="mfv-danger">
            <i class="ti ti-trash" aria-hidden="true"></i> Delete form</a>
        </div>
      </div>
    </div>
  </div>

  <!-- ===== ROW 1: Form details + Pie/Stats ===== -->
  <div class="mfv-main-grid">

    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-purple">
            <i class="ti ti-info-circle" aria-hidden="true"></i></span>
          <div><h3>Form details</h3>
            <p>Basic information and settings</p></div>
        </div>
      </div>
      <div class="mfv-card-body">

        <?php
        $shown = 0;
        $more = 0;
        $cutoff = 5;
        foreach ($members as $m):
          if ($shown >= $cutoff) {
            $more++;
            continue;
          }
          $shown++;
          $role = (int) $m['roles'];
          $mstatus = (int) $m['status'];
          $name = !empty($m['name']) ? $m['name'] : '?';
          $kind = $m['kind'];  // 'member' or 'invite'

          $parts = explode(' ', trim($name));
          $initials = strtoupper(
            substr($parts[0], 0, 1) .
            (isset($parts[1]) ? substr($parts[1], 0, 1) : '')
          );
          ?>
          <div class="mfv-member-row">

            <div
              class="mfv-avatar <?php print $kind === 'invite' ? 'mfv-av-invite' : 'mfv-av-' . $role; ?>">
              <?php if ($kind === 'invite'): ?>
                <i class="ti ti-mail" aria-hidden="true"></i>
              <?php else: ?>
                <?php print $initials; ?>
              <?php endif; ?>
            </div>

            <div class="mfv-member-info">
              <span
                class="mfv-member-name"><?php print check_plain($name); ?></span>
              <span class="mfv-member-role">
          <?php if ($kind === 'invite'): ?>
            Invite pending
          <?php else: ?>
            <?php print $display['role_labels'][$role]; ?>
          <?php endif; ?>
        </span>
            </div>

            <?php if ($kind === 'invite'): ?>
              <span class="mfv-badge mfv-badge-gray">
          <i class="ti ti-clock" aria-hidden="true"></i> Awaiting
        </span>
            <?php elseif ($role !== 0): ?>
              <span
                class="mfv-badge <?php print $display['member_status_classes'][$mstatus]; ?>">
          <?php print $display['member_status_labels'][$mstatus]; ?>
        </span>
            <?php endif; ?>

          </div>
        <?php endforeach; ?>

        <?php if ($more > 0): ?>
          <a href="<?php print $urls['members']; ?>" class="mfv-members-more">
            +<?php print $more; ?> more
            <i class="ti ti-arrow-right" aria-hidden="true"></i>
          </a>
        <?php endif; ?>

        <?php if ($counts['invite_count'] > 0): ?>
          <div class="mfv-invite-note">
            <i class="ti ti-mail" aria-hidden="true"></i>
            <?php print $counts['invite_count']; ?> pending
            invite<?php print $counts['invite_count'] !== 1 ? 's' : ''; ?>
            awaiting registration
          </div>
        <?php endif; ?>

      </div>
    </div>

    <div class="mfv-side-col">

      <div class="mfv-card">
        <div class="mfv-card-head">
          <div class="mfv-card-head-left">
            <span class="mfv-card-icon mfv-ci-teal">
              <i class="ti ti-chart-pie" aria-hidden="true"></i></span>
            <div><h3>Responses</h3>
              <p>Submitted vs pending</p></div>
          </div>
          <span
            class="mfv-badge mfv-badge-purple"><?php print $counts['total']; ?> total</span>
        </div>
        <div class="mfv-card-body">
          <div class="mfv-pie-area">
            <svg class="mfv-pie-svg" viewBox="0 0 120 120" aria-hidden="true">
              <circle cx="60" cy="60" r="50" fill="none" stroke="#EEEDFE"
                      stroke-width="20"/>
              <?php if ($counts['total'] > 0): ?>
                <circle cx="60" cy="60" r="50" fill="none" stroke="#534AB7"
                        stroke-width="20"
                        stroke-dasharray="<?php print $donut['submitted_arc']; ?> <?php print $donut['C']; ?>"
                        stroke-dashoffset="-31.4"
                        transform="rotate(-90 60 60)"/>
                <circle cx="60" cy="60" r="50" fill="none" stroke="#9FE1CB"
                        stroke-width="20"
                        stroke-dasharray="<?php print $donut['pending_arc']; ?> <?php print $donut['C']; ?>"
                        stroke-dashoffset="<?php print (-31.4 - $donut['submitted_arc']); ?>"
                        transform="rotate(-90 60 60)"/>
              <?php endif; ?>
              <text x="60" y="55" text-anchor="middle" font-size="16"
                    font-weight="500"
                    fill="var(--mfv-text-primary)"><?php print $counts['submitted']; ?></text>
              <text x="60" y="70" text-anchor="middle" font-size="10"
                    fill="var(--mfv-text-secondary)">submitted
              </text>
            </svg>
            <div class="mfv-pie-legend">
              <div class="mfv-pie-legend-row">
                <span class="mfv-pie-label-group">
                  <span class="mfv-pie-dot" style="background:#534AB7"></span> Submitted</span>
                <span class="mfv-pie-count"><?php print $counts['submitted']; ?> <span
                    class="mfv-muted">(<?php print $counts['pct']; ?>%)</span></span>
              </div>
              <?php if ((int) $form['visibility'] !== 0): ?>
                <div class="mfv-pie-legend-row">
                  <span class="mfv-pie-label-group">
                    <span class="mfv-pie-dot" style="background:#9FE1CB"></span> Pending</span>
                  <span class="mfv-pie-count"><?php print $counts['pending']; ?>
                      <span
                        class="mfv-muted">(<?php print (100 - $counts['pct']); ?>%)</span>
                  </span>
                </div>
              <?php endif; ?>
            </div>
          </div>
          <div class="mfv-completion-wrap">
            <div class="mfv-completion-label">
              <span><?php print $counts['pct']; ?>% completion rate</span>
              <span
                class="mfv-muted"><?php print $counts['submitted']; ?> of <?php print $counts['total']; ?> members</span>
            </div>
            <div class="mfv-completion-bar-track">
              <div class="mfv-completion-bar-fill"
                   style="width:<?php print $counts['pct']; ?>%"></div>
            </div>
          </div>
        </div>
      </div>

      <div class="mfv-card">
        <div class="mfv-card-head">
          <div class="mfv-card-head-left">
            <span class="mfv-card-icon mfv-ci-blue">
              <i class="ti ti-device-analytics" aria-hidden="true"></i></span>
            <div><h3>Quick stats</h3></div>
          </div>
        </div>
        <div class="mfv-card-body mfv-stats-grid">
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-calendar"
                                           aria-hidden="true"></i> Last response
            </div>
            <div
              class="mfv-stat-value"><?php print $display['last_response']; ?></div>
          </div>
          <?php if ((int) $form['visibility'] !== 0): ?>
            <div class="mfv-stat-mini">
              <div class="mfv-stat-label"><i class="ti ti-mail"
                                             aria-hidden="true"></i> Notified
              </div>
              <div
                class="mfv-stat-value"><?php print (int) $stats['notified']; ?>
                / <?php print $counts['total']; ?></div>
              <div class="mfv-stat-sub">invites sent</div>
            </div>
          <?php endif; ?>
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-flag"
                                           aria-hidden="true"></i> Days left
            </div>
            <div class="mfv-stat-value">
              <?php print $stats['days_left'] !== NULL ? (int) $stats['days_left'] : '∞'; ?>
            </div>
            <div class="mfv-stat-sub">until closing</div>
          </div>
        </div>
      </div>

    </div>
  </div><!-- /row 1 -->

  <!-- ===== ROW 2: Timeline ===== -->
  <div class="mfv-analytics-grid">
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-blue">
            <i class="ti ti-chart-bar" aria-hidden="true"></i></span>
          <div><h3>Submissions over time</h3>
            <p>When people filled the form</p></div>
        </div>
        <span class="mfv-muted" style="font-size:12px">Last 30 days</span>
      </div>
      <div class="mfv-card-body">
        <?php if (!empty($timeline)): ?>
          <div class="mfv-timeline-wrap" role="img"
               aria-label="Submissions timeline chart">
            <div class="mfv-bars">
              <?php foreach ($timeline as $bucket): ?>
                <?php $h = $counts['max_count'] > 0 ? (int) round(60 * ((int) $bucket['count'] / $counts['max_count'])) : 0; ?>
                <div class="mfv-bar-col">
                  <div
                    class="mfv-bar-tooltip"><?php print (int) $bucket['count']; ?></div>
                  <div class="mfv-bar"
                       style="height:<?php print max($h, 2); ?>px"
                       title="<?php print check_plain($bucket['label']); ?>: <?php print (int) $bucket['count']; ?>"></div>
                  <div
                    class="mfv-bar-label"><?php print check_plain($bucket['label']); ?></div>
                </div>
              <?php endforeach; ?>
            </div>
          </div>
        <?php else: ?>
          <div class="mfv-empty">No submission data yet.</div>
        <?php endif; ?>
      </div>
    </div>
  </div><!-- /row 2 -->

  <!-- ===== ROW 3: Questions ===== -->
  <div class="mfv-analytics-grid">
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-purple">
            <i class="ti ti-list" aria-hidden="true"></i></span>
          <div>
            <h3>Questions</h3>
            <p><?php print count($questions); ?> questions</p>
          </div>
        </div>
        <span class="mfv-badge mfv-badge-purple">
          <i class="ti ti-filter" aria-hidden="true"></i>
          <?php print $counts['filterable_count']; ?> filterable
        </span>
      </div>
      <div class="mfv-card-body">
        <div class="mfv-questions-list" role="list">
          <?php foreach ($questions as $q): ?>
            <div class="mfv-q-row" role="listitem">
              <span
                class="mfv-q-label"><?php print check_plain($q['label']); ?></span>
              <?php if (!empty($q['is_filterable'])): ?>
                <span class="mfv-q-filter-badge">
                  <i class="ti ti-filter" aria-hidden="true"></i> filterable
                </span>
              <?php endif; ?>
              <?php if (!empty($q['is_required'])): ?>
                <span class="mfv-q-required" title="Required">*</span>
              <?php endif; ?>
            </div>
          <?php endforeach; ?>
        </div>
        <div class="mfv-filter-row">
          <div class="mfv-filter-chips">
            <?php foreach ($questions as $q): ?>
              <?php if (!empty($q['is_filterable'])): ?>
                <span class="mfv-filter-chip">
                  <i class="ti ti-circle-filled" style="font-size:7px"
                     aria-hidden="true"></i>
                  <?php print check_plain($q['label']); ?>
                </span>
              <?php endif; ?>
            <?php endforeach; ?>
          </div>
          <button class="mfv-btn mfv-btn-sm" id="mfv-filter-toggle"
                  aria-expanded="false">
            <i class="ti ti-adjustments-horizontal" aria-hidden="true"></i>
            Manage filterable questions
          </button>
        </div>
        <?php print drupal_render($filterable_form); ?>
      </div>
    </div>
  </div><!-- /row 3 -->

  <!-- ===== ROW 4: Word cloud + Members ===== -->
  <div class="mfv-bottom-grid">
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-amber">
            <i class="ti ti-tag" aria-hidden="true"></i></span>
          <div><h3>Answer word cloud</h3>
            <p>From open-ended responses</p></div>
        </div>
      </div>
      <div class="mfv-card-body">
        <div class="mfv-wordcloud" id="mfv-wordcloud">
          <?php if (!empty($tag_cloud)): ?>
            <div class="mfv-wordcloud" id="mf-wordcloud"
                 style="width:100%;height:300px;"></div>
          <?php else: ?>
            <span class="mfv-wc-placeholder">Word cloud will appear here once open-ended responses are collected.</span>
          <?php endif; ?>
        </div>
      </div>
    </div>

    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-coral">
            <i class="ti ti-users" aria-hidden="true"></i></span>
          <div><h3>Members</h3>
            <p>Viewers &amp; participants</p></div>
        </div>
        <a href="<?php print $urls['members']; ?>" class="mfv-btn mfv-btn-sm">
          <i class="ti ti-user-plus" aria-hidden="true"></i> Manage
        </a>
      </div>
      <div class="mfv-card-body">
        <?php
        $shown = 0;
        foreach ($members as $m):
          if ($shown >= 5) {
            break;
          }
          $shown++;
          $role = (int) $m['roles'];
          $mstatus = (int) $m['status'];
          $name = !empty($m['name']) ? $m['name'] : '?';
          $parts = explode(' ', trim($name));
          $initials = strtoupper(
            substr($parts[0], 0, 1) . (isset($parts[1]) ? substr($parts[1], 0, 1) : '')
          );
          ?>
          <div class="mfv-member-row">
            <div
              class="mfv-avatar mfv-av-<?php print $role; ?>"><?php print $initials; ?></div>
            <div class="mfv-member-info">
              <span
                class="mfv-member-name"><?php print check_plain($name); ?></span>
              <span
                class="mfv-member-role"><?php print $display['role_labels'][$role]; ?></span>
            </div>
            <?php if ($role !== 0): ?>
              <span
                class="mfv-badge <?php print $display['member_status_classes'][$mstatus]; ?>">
                <?php print $display['member_status_labels'][$mstatus]; ?>
              </span>
            <?php endif; ?>
          </div>
        <?php endforeach; ?>
        <?php if (count($members) > 5): ?>
          <a href="<?php print $urls['members']; ?>" class="mfv-members-more">
            +<?php print (count($members) - 5); ?> more members
            <i class="ti ti-arrow-right" aria-hidden="true"></i>
          </a>
        <?php endif; ?>
      </div>
    </div>

  </div><!-- /row 4 -->

</div><!-- /mfv-page -->
