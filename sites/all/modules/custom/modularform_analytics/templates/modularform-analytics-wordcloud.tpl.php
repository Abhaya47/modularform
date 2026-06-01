<?php
/**
 * @file
 * modularform-analytics-wordcloud.tpl.php
 *
 * Template for the tag word cloud page.
 * JS reads Drupal.settings.modularformWordCloud.words and renders into #mf-wordcloud.
 */
?>
<div class="mf-wordcloud-page">
  <h2><?php print t('Tag Usage Word Cloud'); ?></h2>
  <p class="mf-wordcloud-hint">
    <?php print t('Larger words indicate more frequently used tags.'); ?>
  </p>
  <div id="mf-wordcloud" class="mf-wordcloud-container"></div>
</div>
