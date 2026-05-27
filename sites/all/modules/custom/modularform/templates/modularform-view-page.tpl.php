<?php
/**
 * @file modularform-view-page.tpl.php
 *
 * Variables available:
 *   $form         – associative array of form fields from modularform_forms
 *   $questions    – array of question rows from modularform_questions
 *   $members      – array of member rows from modularform_form_members (with user data joined)
 *   $stats        – associative array: total, submitted, pending, notified, avg_time_seconds, last_submitted_at, days_left
 *   $timeline     – array of ['label' => string, 'count' => int, 'period' => string]
 *                   period is the currently selected grouping: hour|day|week|month
 *   $timeline_period – currently active period string (hour|day|week|month)
 *   $filterable_form – rendered Drupal AJAX form for toggling is_filterable on questions
 *   $form_url     – base URL of the form view page (used for links)
 */

$status_label = ['Drafted', 'Published'];
$status_class = ['mfv-badge-amber', 'mfv-badge-green'];
$visibility_label = ['Public', 'Restricted', 'Private'];
$visibility_icon  = ['ti-world', 'ti-lock-open', 'ti-lock'];

$created_date   = !empty($form['created_at'])    ? date('M j, Y', $form['created_at'])    : '—';
$closes_date    = !empty($form['closes_at'])      ? date('M j, Y g:i A', $form['closes_at']) : '—';
$scheduled_date = !empty($form['scheduled_date']) ? date('M j, Y g:i A', $form['scheduled_date']) : null;
$last_response  = !empty($stats['last_submitted_at']) ? format_interval(REQUEST_TIME - $stats['last_submitted_at'], 1) . ' ago' : 'No responses yet';

$submitted   = (int) $stats['submitted'];
$total       = (int) $stats['total'];
$pending     = $total - $submitted;
$pct         = $total > 0 ? round($submitted / $total * 100) : 0;

// Pie SVG math (donut, r=50, circumference ~314.16)
$C = 314.16;
$submitted_arc = $total > 0 ? round($C * ($submitted / $total), 2) : 0;
$pending_arc   = round($C - $submitted_arc, 2);
$submitted_offset = -31.4; // start at top

$filterable_count = 0;
foreach ($questions as $q) {
  if (!empty($q['is_filterable'])) $filterable_count++;
}

// Timeline bar heights (normalise to 60px)
$max_count = 1;
foreach ($timeline as $t) { $max_count = max($max_count, (int)$t['count']); }
?>
<div class="mfv-page">

  <!-- ===== TOP BAR ===== -->
  <div class="mfv-topbar">
    <div class="mfv-topbar-left">
      <h1 class="mfv-title"><?php print check_plain($form['title']); ?></h1>
      <div class="mfv-meta">
        <span class="mfv-badge <?php print $status_class[$form['status']]; ?>">
          <i class="ti ti-circle-filled" aria-hidden="true"></i>
          <?php print $status_label[$form['status']]; ?>
        </span>
        <span class="mfv-badge mfv-badge-gray">
          <i class="ti <?php print $visibility_icon[$form['visibility']]; ?>" aria-hidden="true"></i>
          <?php print $visibility_label[$form['visibility']]; ?>
        </span>
        <?php if ($form['slug']): ?>
          <span class="mfv-meta-slug">slug: <code><?php print check_plain($form['slug']); ?></code></span>
        <?php endif; ?>
        <?php if ($form['closes'] && $form['closes_at']): ?>
          <span class="mfv-meta-text">
            <i class="ti ti-clock" aria-hidden="true"></i>
            Closes <?php print $closes_date; ?>
          </span>
        <?php endif; ?>
      </div>
    </div>
    <div class="mfv-topbar-actions">
      <a href="<?php print $form_url; ?>/preview" class="mfv-btn mfv-btn-sm" target="_blank">
        <i class="ti ti-external-link" aria-hidden="true"></i> Preview
      </a>
      <div class="mfv-dropdown-wrap" id="mfv-actions-wrap">
        <button class="mfv-btn mfv-btn-sm" id="mfv-actions-toggle" aria-haspopup="true" aria-expanded="false">
          <i class="ti ti-dots-vertical" aria-hidden="true"></i> Actions
          <i class="ti ti-chevron-down" aria-hidden="true"></i>
        </button>
        <div class="mfv-dropdown-menu" id="mfv-actions-menu" role="menu" hidden>
          <a href="<?php print $form_url; ?>/edit" role="menuitem">
            <i class="ti ti-edit" aria-hidden="true"></i> Edit form
          </a>
          <a href="<?php print $form_url; ?>/settings" role="menuitem">
            <i class="ti ti-settings" aria-hidden="true"></i> Settings
          </a>
          <a href="<?php print $form_url; ?>/members" role="menuitem">
            <i class="ti ti-users" aria-hidden="true"></i> Manage members
          </a>
          <a href="<?php print $form_url; ?>/invites" role="menuitem">
            <i class="ti ti-mail" aria-hidden="true"></i> Send invites
          </a>
          <a href="<?php print $form_url; ?>/export" role="menuitem">
            <i class="ti ti-download" aria-hidden="true"></i> Export responses
          </a>
          <a href="<?php print $form_url; ?>/delete" role="menuitem" class="mfv-danger">
            <i class="ti ti-trash" aria-hidden="true"></i> Delete form
          </a>
        </div>
      </div>
    </div>
  </div>

  <!-- ===== ROW 1: Form details + Responses pie ===== -->
  <div class="mfv-main-grid">

    <!-- Form details -->
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-purple"><i class="ti ti-info-circle" aria-hidden="true"></i></span>
          <div>
            <h3>Form details</h3>
            <p>Basic information and settings</p>
          </div>
        </div>
      </div>
      <div class="mfv-card-body">
        <?php if (!empty($form['description'])): ?>
          <div class="mfv-info-row">
            <span class="mfv-info-label"><i class="ti ti-align-left" aria-hidden="true"></i> Description</span>
            <span class="mfv-info-value mfv-info-desc"><?php print check_markup($form['description']); ?></span>
          </div>
        <?php endif; ?>
        <div class="mfv-info-row">
          <span class="mfv-info-label"><i class="ti ti-calendar" aria-hidden="true"></i> Created</span>
          <span class="mfv-info-value"><?php print $created_date; ?></span>
        </div>
        <div class="mfv-info-row">
          <span class="mfv-info-label"><i class="ti ti-clock" aria-hidden="true"></i> Closes at</span>
          <span class="mfv-info-value">
            <?php if ($form['closes'] && $form['closes_at']): ?>
              <?php print $closes_date; ?>
            <?php else: ?>
              <span class="mfv-badge mfv-badge-gray">Never</span>
            <?php endif; ?>
          </span>
        </div>
        <div class="mfv-info-row">
          <span class="mfv-info-label"><i class="ti ti-calendar-event" aria-hidden="true"></i> Scheduled</span>
          <span class="mfv-info-value">
            <?php if ($form['scheduled'] && $scheduled_date): ?>
              <?php print $scheduled_date; ?>
            <?php else: ?>
              <span class="mfv-badge mfv-badge-gray">Not scheduled</span>
            <?php endif; ?>
          </span>
        </div>
        <div class="mfv-info-row">
          <span class="mfv-info-label"><i class="ti ti-pencil" aria-hidden="true"></i> Editable responses</span>
          <span class="mfv-info-value">
            <?php if ($form['editable']): ?>
              <span class="mfv-badge mfv-badge-green">Enabled</span>
            <?php else: ?>
              <span class="mfv-badge mfv-badge-gray">Disabled</span>
            <?php endif; ?>
          </span>
        </div>
        <div class="mfv-info-row">
          <span class="mfv-info-label"><i class="ti ti-ghost" aria-hidden="true"></i> Anonymous responses</span>
          <span class="mfv-info-value">
            <?php if ($form['anonymous_responses']): ?>
              <span class="mfv-badge mfv-badge-green">Enabled</span>
            <?php else: ?>
              <span class="mfv-badge mfv-badge-amber">Disabled</span>
            <?php endif; ?>
          </span>
        </div>
      </div>
    </div>

    <!-- Right column: Pie + Completion + Quick Stats -->
    <div class="mfv-side-col">

      <!-- Responses pie -->
      <div class="mfv-card">
        <div class="mfv-card-head">
          <div class="mfv-card-head-left">
            <span class="mfv-card-icon mfv-ci-teal"><i class="ti ti-chart-pie" aria-hidden="true"></i></span>
            <div>
              <h3>Responses</h3>
              <p>Submitted vs pending</p>
            </div>
          </div>
          <span class="mfv-badge mfv-badge-purple"><?php print $total; ?> total</span>
        </div>
        <div class="mfv-card-body">
          <div class="mfv-pie-area">
            <svg class="mfv-pie-svg" viewBox="0 0 120 120" aria-hidden="true">
              <circle cx="60" cy="60" r="50" fill="none" stroke="#EEEDFE" stroke-width="20"/>
              <?php if ($total > 0): ?>
                <circle cx="60" cy="60" r="50" fill="none" stroke="#534AB7" stroke-width="20"
                        stroke-dasharray="<?php print $submitted_arc; ?> <?php print $C; ?>"
                        stroke-dashoffset="<?php print $submitted_offset; ?>"
                        transform="rotate(-90 60 60)"/>
                <circle cx="60" cy="60" r="50" fill="none" stroke="#9FE1CB" stroke-width="20"
                        stroke-dasharray="<?php print $pending_arc; ?> <?php print $C; ?>"
                        stroke-dashoffset="<?php print -31.4 - $submitted_arc; ?>"
                        transform="rotate(-90 60 60)"/>
              <?php endif; ?>
              <text x="60" y="55" text-anchor="middle" font-size="16" font-weight="500" fill="var(--mfv-text-primary)"><?php print $submitted; ?></text>
              <text x="60" y="70" text-anchor="middle" font-size="10" fill="var(--mfv-text-secondary)">submitted</text>
            </svg>
            <div class="mfv-pie-legend">
              <div class="mfv-pie-legend-row">
                <span class="mfv-pie-label-group"><span class="mfv-pie-dot" style="background:#534AB7"></span> Submitted</span>
                <span class="mfv-pie-count"><?php print $submitted; ?> <span class="mfv-muted">(<?php print $pct; ?>%)</span></span>
              </div>
              <div class="mfv-pie-legend-row">
                <span class="mfv-pie-label-group"><span class="mfv-pie-dot" style="background:#9FE1CB"></span> Pending</span>
                <span class="mfv-pie-count"><?php print $pending; ?> <span class="mfv-muted">(<?php print (100 - $pct); ?>%)</span></span>
              </div>
            </div>
          </div>

          <!-- Completion bar -->
          <div class="mfv-completion-wrap">
            <div class="mfv-completion-label">
              <span><?php print $pct; ?>% completion rate</span>
              <span class="mfv-muted"><?php print $submitted; ?> of <?php print $total; ?> members</span>
            </div>
            <div class="mfv-completion-bar-track">
              <div class="mfv-completion-bar-fill" style="width:<?php print $pct; ?>%"></div>
            </div>
          </div>
        </div>
      </div>

      <!-- Quick stats -->
      <div class="mfv-card">
        <div class="mfv-card-head">
          <div class="mfv-card-head-left">
            <span class="mfv-card-icon mfv-ci-blue"><i class="ti ti-device-analytics" aria-hidden="true"></i></span>
            <div><h3>Quick stats</h3></div>
          </div>
        </div>
        <div class="mfv-card-body mfv-stats-grid">
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-clock" aria-hidden="true"></i> Avg. time</div>
            <div class="mfv-stat-value">
              <?php
              $avg = (int)$stats['avg_time_seconds'];
              print $avg > 0 ? floor($avg/60) . 'm ' . ($avg%60) . 's' : '—';
              ?>
            </div>
            <div class="mfv-stat-sub">to complete</div>
          </div>
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-calendar" aria-hidden="true"></i> Last response</div>
            <div class="mfv-stat-value"><?php print $last_response; ?></div>
          </div>
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-mail" aria-hidden="true"></i> Notified</div>
            <div class="mfv-stat-value"><?php print (int)$stats['notified']; ?> / <?php print $total; ?></div>
            <div class="mfv-stat-sub">invites sent</div>
          </div>
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-flag" aria-hidden="true"></i> Days left</div>
            <div class="mfv-stat-value">
              <?php print ($stats['days_left'] !== null) ? (int)$stats['days_left'] : '∞'; ?>
            </div>
            <div class="mfv-stat-sub">until closing</div>
          </div>
        </div>
      </div>

    </div><!-- /side col -->
  </div><!-- /main-grid -->

  <!-- ===== ROW 2: Timeline + Word cloud ===== -->
  <div class="mfv-analytics-grid">

    <!-- Submissions timeline -->
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-blue"><i class="ti ti-chart-bar" aria-hidden="true"></i></span>
          <div>
            <h3>Submissions over time</h3>
            <p>When people filled the form</p>
          </div>
        </div>
        <!-- Period filter tabs -->
        <div class="mfv-period-tabs" role="group" aria-label="Timeline grouping">
          <?php foreach (['hour', 'day', 'week', 'month'] as $p): ?>
            <a href="<?php print $form_url; ?>?period=<?php print $p; ?>"
               class="mfv-period-tab<?php if ($timeline_period === $p) print ' mfv-period-tab--active'; ?>"
               aria-current="<?php print ($timeline_period === $p) ? 'true' : 'false'; ?>">
              <?php print ucfirst($p); ?>
            </a>
          <?php endforeach; ?>
        </div>
      </div>
      <div class="mfv-card-body">
        <?php if (!empty($timeline)): ?>
          <div class="mfv-timeline-wrap" role="img" aria-label="Submissions timeline chart">
            <div class="mfv-bars">
              <?php foreach ($timeline as $bucket): ?>
                <?php $h = $max_count > 0 ? round(60 * ((int)$bucket['count'] / $max_count)) : 0; ?>
                <div class="mfv-bar-col">
                  <div class="mfv-bar-tooltip"><?php print (int)$bucket['count']; ?></div>
                  <div class="mfv-bar" style="height:<?php print max($h, 2); ?>px"
                       title="<?php print check_plain($bucket['label']); ?>: <?php print (int)$bucket['count']; ?>"></div>
                  <div class="mfv-bar-label"><?php print check_plain($bucket['label']); ?></div>
                </div>
              <?php endforeach; ?>
            </div>
          </div>
        <?php else: ?>
          <div class="mfv-empty">No submission data yet.</div>
        <?php endif; ?>
      </div>
    </div>

    <!-- Word cloud (static placeholder) -->
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-amber"><i class="ti ti-tag" aria-hidden="true"></i></span>
          <div>
            <h3>Answer word cloud</h3>
            <p>From open-ended responses</p>
          </div>
        </div>
      </div>
      <div class="mfv-card-body">
        <div class="mfv-wordcloud" id="mfv-wordcloud">
          <!-- Populated by JS / future backend integration -->
          <span class="mfv-wc-placeholder">Word cloud will appear here once open-ended responses are collected.</span>
        </div>
      </div>
    </div>

  </div><!-- /analytics-grid -->

  <!-- ===== ROW 3: Questions + Members ===== -->
  <div class="mfv-main-grid">

    <!-- Questions + filterable management -->
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-purple"><i class="ti ti-list" aria-hidden="true"></i></span>
          <div>
            <h3>Questions</h3>
            <p><?php print count($questions); ?> questions</p>
          </div>
        </div>
        <span class="mfv-badge mfv-badge-purple">
          <i class="ti ti-filter" aria-hidden="true"></i>
          <?php print $filterable_count; ?> filterable
        </span>
      </div>
      <div class="mfv-card-body">

        <!-- Question list -->
        <div class="mfv-questions-list" role="list">
          <?php foreach ($questions as $i => $q): ?>
            <div class="mfv-q-row" role="listitem">
              <span class="mfv-q-index"><?php print ($i + 1); ?></span>
              <span class="mfv-q-label"><?php print check_plain($q['label']); ?></span>
              <span class="mfv-q-type"><?php print check_plain($q['type']); ?></span>
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

        <!-- Active filter chips -->
        <div class="mfv-filter-row">
          <div class="mfv-filter-chips" id="mfv-filter-chips">
            <?php foreach ($questions as $q): ?>
              <?php if (!empty($q['is_filterable'])): ?>
                <span class="mfv-filter-chip">
                  <i class="ti ti-circle-filled" style="font-size:7px" aria-hidden="true"></i>
                  <?php print check_plain($q['label']); ?>
                </span>
              <?php endif; ?>
            <?php endforeach; ?>
          </div>
          <button class="mfv-btn mfv-btn-sm" id="mfv-filter-toggle"
                  aria-controls="mfv-filter-overlay" aria-expanded="false">
            <i class="ti ti-adjustments-horizontal" aria-hidden="true"></i>
            Manage filterable questions
          </button>
        </div>

        <!-- Filterable questions AJAX overlay (rendered by Drupal form) -->
        <div class="mfv-filter-overlay" id="mfv-filter-overlay" hidden aria-live="polite">
          <?php print $filterable_form; ?>
        </div>

      </div><!-- /card-body -->
    </div><!-- /questions card -->

    <!-- Members -->
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-coral"><i class="ti ti-users" aria-hidden="true"></i></span>
          <div>
            <h3>Members</h3>
            <p>Viewers &amp; participants</p>
          </div>
        </div>
        <a href="<?php print $form_url; ?>/members" class="mfv-btn mfv-btn-sm">
          <i class="ti ti-user-plus" aria-hidden="true"></i> Manage
        </a>
      </div>
      <div class="mfv-card-body">
        <?php
        $role_labels = ['Creator', 'Viewer', 'Participant'];
        $status_label_m = ['Pending', 'Submitted'];
        $status_badge_m = ['mfv-badge-amber', 'mfv-badge-green'];
        $shown = 0;
        ?>
        <?php foreach ($members as $m): ?>
          <?php if ($shown >= 5): break; endif; $shown++; ?>
          <div class="mfv-member-row">
            <div class="mfv-avatar mfv-av-<?php print $m['role']; ?>">
              <?php
              $name = !empty($m['name']) ? $m['name'] : '?';
              $parts = explode(' ', trim($name));
              print strtoupper(substr($parts[0], 0, 1) . (isset($parts[1]) ? substr($parts[1], 0, 1) : ''));
              ?>
            </div>
            <div class="mfv-member-info">
              <span class="mfv-member-name"><?php print check_plain($name); ?></span>
              <span class="mfv-member-role"><?php print $role_labels[(int)$m['roles']]; ?></span>
            </div>
            <?php if ((int)$m['roles'] !== 0): ?>
              <span class="mfv-badge <?php print $status_badge_m[(int)$m['status']]; ?>">
                <?php print $status_label_m[(int)$m['status']]; ?>
              </span>
            <?php endif; ?>
          </div>
        <?php endforeach; ?>
        <?php if (count($members) > 5): ?>
          <a href="<?php print $form_url; ?>/members" class="mfv-members-more">
            +<?php print (count($members) - 5); ?> more members
            <i class="ti ti-arrow-right" aria-hidden="true"></i>
          </a>
        <?php endif; ?>
      </div>
    </div>

  </div><!-- /main-grid -->

</div><!-- /mfv-page -->
