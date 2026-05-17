<h2 class="sr-only"><?php print $form_title; ?></h2>

<div class="mf-create-page">

  <div class="mf-page-header">
    <h1><?php print isset($form['id']['#default_value']) && $form['id']['#default_value'] ? t('Edit form') : t('Create a new form'); ?></h1>
    <p><?php print t('Fill in the details below to set up your form before building.'); ?></p>
  </div>

  <form<?php print drupal_attributes($form['#attributes']); ?>>
    <?php print render($form['id']); ?>
    <?php print render($form['form_build_id']); ?>
    <?php print render($form['form_token']); ?>
    <?php print render($form['form_id']); ?>

    <?php // ── Section 1: Basics ───────────────────────────────────────────── ?>
    <div class="mf-section">
      <div class="mf-section-head">
        <div class="mf-section-icon mf-icon-purple">
          <span class="mf-icon">&#9998;</span>
        </div>
        <div class="mf-section-head-text">
          <h3><?php print t('Basics'); ?></h3>
          <p><?php print t('Name, description, and publication status'); ?></p>
        </div>
      </div>
      <div class="mf-section-body">
        <div class="mf-field-grid mf-col-2">
          <div class="mf-field">
            <?php print render($form['title']); ?>
          </div>
          <div class="mf-field">
            <?php print render($form['description']); ?>
          </div>
        </div>
        <div class="mf-field mf-field--status">
          <label class="mf-label"><?php print t('Status'); ?></label>
          <p
            class="mf-field-hint"><?php print t('Draft forms are only visible to you. Switch to Published when ready to share.'); ?></p>
          <?php print render($form['status']); ?>
        </div>
      </div>
    </div>


    <?php if (isset($form['id']['#default_value']) && $form['id']['#default_value']) : ?>
      <?php // ── Section 2: Visibility & access ──────────────────────────────── ?>
      <div class="mf-section">
        <div class="mf-section-head">
          <div class="mf-section-icon mf-icon-teal">
            <span class="mf-icon">&#128065;</span>
          </div>
          <div class="mf-section-head-text">
            <h3><?php print t('Visibility & access'); ?></h3>
            <p><?php print t('Control who can see and submit this form'); ?></p>
          </div>
        </div>
        <div class="mf-section-body">
          <div class="mf-field">
            <label
              class="mf-label"><?php print t('Who can access this form?'); ?></label>
            <p
              class="mf-field-hint"><?php print t('Public forms can be filled by anyone with the link. Restricted limits by role. Private invites specific users.'); ?></p>
            <?php print render($form['visibility']); ?>
          </div>
          <div class="mf-visibility-extra">
            <?php print render($form['visibility_options_wrapper']); ?>
          </div>
        </div>
      </div>
    <?php else : ?>
    <?php // Store visibility default silently so it saves as public on create ?>
    <?php $form['visibility']['#access'] = FALSE; ?>
    <?php $form['visibility_options_wrapper']['#access'] = FALSE; ?>
    <?php print render($form['visibility']); ?>
    <?php print render($form['visibility_options_wrapper']); ?>
    <?php endif; ?>

    <?php // ── Section 3: Scheduling ────────────────────────────────────────── ?>
    <div class="mf-section">
      <div class="mf-section-head">
        <div class="mf-section-icon mf-icon-amber">
          <span class="mf-icon">&#128197;</span>
        </div>
        <div class="mf-section-head-text">
          <h3><?php print t('Scheduling'); ?></h3>
          <p><?php print t('When the form becomes available and when it closes'); ?></p>
        </div>
      </div>
      <div class="mf-section-body">

        <div class="mf-toggle-row">
          <div class="mf-toggle-info">
            <h4><?php print t('Schedule a publishing date'); ?></h4>
            <p><?php print t('The form will automatically go live on a specific date, rather than immediately.'); ?></p>
          </div>
          <div class="mf-toggle-field">
            <?php print render($form['scheduled']); ?>
          </div>
        </div>
        <div class="mf-date-reveal" id="scheduled-date-wrapper-styled">
          <?php print render($form['scheduled_date_wrapper']); ?>
        </div>

        <div class="mf-toggle-row">
          <div class="mf-toggle-info">
            <h4><?php print t('Set a closing date'); ?></h4>
            <p><?php print t('Responses will no longer be accepted after this date. Useful for deadlines.'); ?></p>
          </div>
          <div class="mf-toggle-field">
            <?php print render($form['closes']); ?>
          </div>
        </div>
        <div class="mf-date-reveal" id="closes-at-wrapper-styled">
          <?php print render($form['closes_at_wrapper']); ?>
        </div>

      </div>
    </div>

    <?php // ── Section 4: Response settings ─────────────────────────────────── ?>
    <div class="mf-section">
      <div class="mf-section-head">
        <div class="mf-section-icon mf-icon-blue">
          <span class="mf-icon">&#9881;</span>
        </div>
        <div class="mf-section-head-text">
          <h3><?php print t('Response settings'); ?></h3>
          <p><?php print t('How responses are collected and managed'); ?></p>
        </div>
      </div>
      <div class="mf-section-body">

        <div class="mf-toggle-row">
          <div class="mf-toggle-info">
            <h4><?php print t('Allow respondents to edit their submission'); ?></h4>
            <p><?php print t('Lets people return and update their answers after submitting.'); ?></p>
          </div>
          <div class="mf-toggle-field">
            <?php print render($form['editable']); ?>
          </div>
        </div>

        <div class="mf-toggle-row">
          <div class="mf-toggle-info">
            <h4><?php print t('Collect responses anonymously'); ?></h4>
            <p><?php print t("Names and user accounts won't be linked to answers. Good for sensitive topics."); ?></p>
          </div>
          <div class="mf-toggle-field">
            <?php print render($form['anonymous_responses']); ?>
          </div>
        </div>

      </div>
    </div>

    <div class="mf-actions">
      <?php print render($form['submit']); ?>
    </div>

    <?php print drupal_render_children($form); ?>
  </form>

</div>
