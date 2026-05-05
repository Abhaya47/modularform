<div class="header">
  <?php if (!empty($form['id']['#default_value'])): ?>
    EDIT Form Info
  <?php else: ?>
    Create Form
  <?php endif; ?>
</div>
<div class="modularform-initial-page">
  <div class="modularform-initial-wrapper">
    <h2><?php print t('Form Info'); ?></h2>
    <h3><?php print t('Please enter initial info for the form'); ?></h3>

    <form<?php print drupal_attributes($form['#attributes']); ?>>

      <div class="form-row">
        <div class="form-field">
          <?php print render($form['title']); ?>
        </div>
      </div>

      <div class="form-row">
        <div class="form-field full">
          <?php print render($form['description']); ?>
        </div>
      </div>


      <div class="form-row">
        <div class="form-field">
          <?php print render($form['status']); ?>
        </div>
        <div class="form-field">
          <?php print render($form['closes']); ?>
        </div>


        <div class="form-field">
          <?php print render($form['closes_at']); ?>
        </div>
      </div>

      <div class="form-actions">
        <?php print render($form['submit']); ?>
      </div>

      <?php print drupal_render_children($form); ?>

    </form>
  </div>

</div>
