<?php
/**
 * @file modularform-view-page.tpl.php
 *
 * Variables:
 *   $form            – row from modularform_forms
 *   $questions       – array of question rows
 *   $members         – array of member rows with 'name' attached
 *   $stats           – total, submitted, pending, notified, avg_time_seconds, last_submitted_at, days_left
 *   $timeline        – array of ['label', 'count']
 *   $timeline_period – hour|day|week|month
 *   $filterable_form – rendered AJAX form HTML
 *   $form_url        – base URL e.g. modularform/42
 */

$status_labels    = ['Drafted', 'Published'];
$status_classes   = ['mfv-badge-amber', 'mfv-badge-green'];
$visibility_labels = ['Public', 'Restricted', 'Private'];
$visibility_icons  = ['ti-world', 'ti-lock-open', 'ti-lock'];
$role_labels      = ['Creator', 'Viewer', 'Participant'];
$member_status_labels = ['Pending', 'Submitted'];
$member_status_classes = ['mfv-badge-amber', 'mfv-badge-green'];

$created_date   = !empty($form['created_at'])     ? date('M j, Y', $form['created_at'])         : '—';
$closes_date    = !empty($form['closes_at'])       ? date('M j, Y g:i A', $form['closes_at'])    : '—';
$scheduled_date = !empty($form['scheduled_date'])  ? date('M j, Y g:i A', $form['scheduled_date']) : NULL;
$last_response  = !empty($stats['last_submitted_at'])
  ? format_interval(REQUEST_TIME - $stats['last_submitted_at'], 1) . ' ago'
  : 'No responses yet';

$submitted = (int) $stats['submitted'];
$total     = (int) $stats['total'];
$pending   = $total - $submitted;
$pct       = $total > 0 ? round($submitted / $total * 100) : 0;

// Pie SVG (donut r=50, circumference=314.16)
$C              = 314.16;
$submitted_arc  = $total > 0 ? round($C * ($submitted / $total), 2) : 0;
$pending_arc    = round($C - $submitted_arc, 2);

$filterable_count = 0;
foreach ($questions as $q) {
  if (!empty($q['is_filterable'])) {
    $filterable_count++;
  }
}

// Normalise bar heights to 60px
$max_count = 1;
foreach ($timeline as $bucket) {
  if ((int) $bucket['count'] > $max_count) {
    $max_count = (int) $bucket['count'];
  }
}
?>
<div class="mfv-page">

  <!-- ===== TOP BAR ===== -->
  <div class="mfv-topbar">
    <div class="mfv-topbar-left">
      <h1 class="mfv-title"><?php print check_plain($form['title']); ?></h1>
      <div class="mfv-meta">
        <span class="mfv-badge <?php print $status_classes[(int) $form['status']]; ?>">
          <i class="ti ti-circle-filled" aria-hidden="true"></i>
          <?php print $status_labels[(int) $form['status']]; ?>
        </span>
        <span class="mfv-badge mfv-badge-gray">
          <i class="ti <?php print $visibility_icons[(int) $form['visibility']]; ?>" aria-hidden="true"></i>
          <?php print $visibility_labels[(int) $form['visibility']]; ?>
        </span>
        <?php if (!empty($form['slug'])): ?>
          <span class="mfv-meta-slug">slug: <code><?php print check_plain($form['slug']); ?></code></span>
        <?php endif; ?>
        <?php if ($form['closes'] && $form['closes_at']): ?>
          <span class="mfv-meta-text">
            <i class="ti ti-clock" aria-hidden="true"></i> Closes <?php print $closes_date; ?>
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
          <a href="<?php print $form_url; ?>/edit" role="menuitem"><i class="ti ti-edit" aria-hidden="true"></i> Edit form</a>
          <a href="<?php print $form_url; ?>/settings" role="menuitem"><i class="ti ti-settings" aria-hidden="true"></i> Settings</a>
          <a href="<?php print $form_url; ?>/members" role="menuitem"><i class="ti ti-users" aria-hidden="true"></i> Manage members</a>
          <a href="<?php print $form_url; ?>/invites" role="menuitem"><i class="ti ti-mail" aria-hidden="true"></i> Send invites</a>
          <a href="<?php print $form_url; ?>/export" role="menuitem"><i class="ti ti-download" aria-hidden="true"></i> Export responses</a>
          <a href="<?php print $form_url; ?>/delete" role="menuitem" class="mfv-danger"><i class="ti ti-trash" aria-hidden="true"></i> Delete form</a>
        </div>
      </div>
    </div>
  </div>

  <!-- ===== ROW 1: Form details (left) + Pie / Stats (right) ===== -->
  <div class="mfv-main-grid">

    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-purple"><i class="ti ti-info-circle" aria-hidden="true"></i></span>
          <div><h3>Form details</h3><p>Basic information and settings</p></div>
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

    <div class="mfv-side-col">

      <!-- Responses pie + completion bar -->
      <div class="mfv-card">
        <div class="mfv-card-head">
          <div class="mfv-card-head-left">
            <span class="mfv-card-icon mfv-ci-teal"><i class="ti ti-chart-pie" aria-hidden="true"></i></span>
            <div><h3>Responses</h3><p>Submitted vs pending</p></div>
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
                        stroke-dashoffset="-31.4"
                        transform="rotate(-90 60 60)"/>
                <circle cx="60" cy="60" r="50" fill="none" stroke="#9FE1CB" stroke-width="20"
                        stroke-dasharray="<?php print $pending_arc; ?> <?php print $C; ?>"
                        stroke-dashoffset="<?php print (-31.4 - $submitted_arc); ?>"
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
              $avg = (int) $stats['avg_time_seconds'];
              print $avg > 0 ? floor($avg / 60) . 'm ' . ($avg % 60) . 's' : '—';
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
            <div class="mfv-stat-value"><?php print (int) $stats['notified']; ?> / <?php print $total; ?></div>
            <div class="mfv-stat-sub">invites sent</div>
          </div>
          <div class="mfv-stat-mini">
            <div class="mfv-stat-label"><i class="ti ti-flag" aria-hidden="true"></i> Days left</div>
            <div class="mfv-stat-value">
              <?php print $stats['days_left'] !== NULL ? (int) $stats['days_left'] : '∞'; ?>
            </div>
            <div class="mfv-stat-sub">until closing</div>
          </div>
        </div>
      </div>

    </div><!-- /mfv-side-col -->
  </div><!-- /mfv-main-grid row 1 -->

  <!-- ===== ROW 2: Timeline + Word cloud ===== -->
  <div class="mfv-analytics-grid">

    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-blue"><i class="ti ti-chart-bar" aria-hidden="true"></i></span>
          <div><h3>Submissions over time</h3><p>When people filled the form</p></div>
        </div>
        <span class="mfv-muted" style="font-size:12px">Last 30 days</span>
      </div>
      <div class="mfv-card-body">
        <?php if (!empty($timeline)): ?>
          <div class="mfv-timeline-wrap" role="img" aria-label="Submissions timeline chart">
            <div class="mfv-bars">
              <?php foreach ($timeline as $bucket): ?>
                <?php $h = $max_count > 0 ? (int) round(60 * ((int) $bucket['count'] / $max_count)) : 0; ?>
                <div class="mfv-bar-col">
                  <div class="mfv-bar-tooltip"><?php print (int) $bucket['count']; ?></div>
                  <div class="mfv-bar" style="height:<?php print max($h, 2); ?>px"
                       title="<?php print check_plain($bucket['label']); ?>: <?php print (int) $bucket['count']; ?>"></div>
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

    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-amber"><i class="ti ti-tag" aria-hidden="true"></i></span>
          <div><h3>Answer word cloud</h3><p>From open-ended responses</p></div>
        </div>
      </div>
      <div class="mfv-card-body">
        <div class="mfv-wordcloud" id="mfv-wordcloud">
          <span class="mfv-wc-placeholder">Word cloud will appear here once open-ended responses are collected.</span>
        </div>
      </div>
    </div>

  </div><!-- /mfv-analytics-grid -->

  <!-- ===== ROW 3: Questions + Members ===== -->
  <div class="mfv-main-grid">

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

        <div class="mfv-questions-list" role="list">
          <?php foreach ($questions as $i => $q): ?>
            <div class="mfv-q-row" role="listitem">
              <span class="mfv-q-label"><?php print check_plain($q['label']); ?></span>
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

        <!-- Active filterable chips + toggle button -->
        <div class="mfv-filter-row">
          <div class="mfv-filter-chips">
            <?php foreach ($questions as $q): ?>
              <?php if (!empty($q['is_filterable'])): ?>
                <span class="mfv-filter-chip">
                  <i class="ti ti-circle-filled" style="font-size:7px" aria-hidden="true"></i>
                  <?php print check_plain($q['label']); ?>
                </span>
              <?php endif; ?>
            <?php endforeach; ?>
          </div>
          <button class="mfv-btn mfv-btn-sm" id="mfv-filter-toggle" aria-expanded="false">
            <i class="ti ti-adjustments-horizontal" aria-hidden="true"></i>
            Manage filterable questions
          </button>
        </div>

        <!-- AJAX filterable form — #prefix owns the wrapper div, JS toggles display -->
        <?php print drupal_render($filterable_form); ?>

      </div>
    </div>

    <!-- Members -->
    <div class="mfv-card">
      <div class="mfv-card-head">
        <div class="mfv-card-head-left">
          <span class="mfv-card-icon mfv-ci-coral"><i class="ti ti-users" aria-hidden="true"></i></span>
          <div><h3>Members</h3><p>Viewers &amp; participants</p></div>
        </div>
        <a href="<?php print $form_url; ?>/members" class="mfv-btn mfv-btn-sm">
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
          $role    = (int) $m['roles'];
          $mstatus = (int) $m['status'];
          $name    = !empty($m['name']) ? $m['name'] : '?';
          $parts   = explode(' ', trim($name));
          $initials = strtoupper(
            substr($parts[0], 0, 1) . (isset($parts[1]) ? substr($parts[1], 0, 1) : '')
          );
          ?>
          <div class="mfv-member-row">
            <div class="mfv-avatar mfv-av-<?php print $role; ?>"><?php print $initials; ?></div>
            <div class="mfv-member-info">
              <span class="mfv-member-name"><?php print check_plain($name); ?></span>
              <span class="mfv-member-role"><?php print $role_labels[$role]; ?></span>
            </div>
            <?php if ($role !== 0): ?>
              <span class="mfv-badge <?php print $member_status_classes[$mstatus]; ?>">
                <?php print $member_status_labels[$mstatus]; ?>
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

  </div><!-- /mfv-main-grid row 3 -->

</div><!-- /mfv-page -->
