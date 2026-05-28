<?php
/**
 * @file modularform-export-page.tpl.php
 * Template for the export confirmation page.
 *
 * Available variables:
 *   $records      — array of stdObjects, keyed by form id.
 *                   Each has: ->id, ->title, ->status, ->created_at
 *   $token        — CSRF token string for the POST form
 *   $fids_hidden  — comma-separated validated fid string for the hidden input
 *   $count        — number of forms being exported
 */
?>

<div class="mf-export-page">

  <p class="mf-export-back">
    <?php print l('← ' . t('Back to forms'), 'forms'); ?>
  </p>

  <p class="mf-export-intro">
    <?php print t('You are about to export the following @count form(s). Choose what to export below.', ['@count' => $count]); ?>
  </p>

  <!-- ════════════════════════════
       SELECTED FORMS LIST
  ════════════════════════════ -->
  <ul class="mf-export-form-list">
    <?php foreach ($records as $form): ?>
      <li>
        <span class="mf-export-form-title">
          <?php print check_plain($form->title); ?>
        </span>
        <span class="mf-export-form-meta">
          <?php print $form->status ? t('Published') : t('Draft'); ?>
          &middot;
          <?php print format_date($form->created_at, 'custom', 'M j, Y'); ?>
        </span>
      </li>
    <?php endforeach; ?>
  </ul>

  <!-- ════════════════════════════
       EXPORT ACTIONS
  ════════════════════════════ -->
  <form method="post" action="" class="mf-export-form">
    <input type="hidden" name="token" value="<?php print check_plain($token); ?>"/>
    <input type="hidden" name="fids"  value="<?php print check_plain($fids_hidden); ?>"/>

    <div class="mf-export-actions">

      <button type="submit" name="op_forms" value="1"
              class="mf-export-btn mf-export-btn--primary">
        <svg viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"
             width="14" height="14" aria-hidden="true">
          <path d="M3 2h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3a1 1 0 0 1 1-1z"
                stroke="currentColor" stroke-width="1.4"/>
          <rect x="4.5" y="5"   width="7" height="1.2" rx=".6" fill="currentColor"/>
          <rect x="4.5" y="7.5" width="7" height="1.2" rx=".6" fill="currentColor"/>
          <rect x="4.5" y="10"  width="4" height="1.2" rx=".6" fill="currentColor"/>
        </svg>
        <?php print t('Export form definitions'); ?>
      </button>

      <button type="submit" name="op_responses" value="1"
              class="mf-export-btn mf-export-btn--secondary">
        <svg viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg"
             width="14" height="14" aria-hidden="true">
          <path d="M8 1.5C4.96 1.5 2.5 3.69 2.5 6.4c0 1.18.46 2.26 1.22 3.1L3 13l3.4-1.28A6.8 6.8 0 0 0 8 11.8c3.04 0 5.5-2.19 5.5-4.9S11.04 1.5 8 1.5Z"
                stroke="currentColor" stroke-width="1.3" stroke-linejoin="round"/>
          <rect x="5.5" y="5.8" width="5" height="1.1" rx=".55" fill="currentColor"/>
          <rect x="5.5" y="7.8" width="3" height="1.1" rx=".55" fill="currentColor"/>
        </svg>
        <?php print t('Export responses'); ?>
      </button>

    </div>
  </form>

</div><!-- /mf-export-page -->
