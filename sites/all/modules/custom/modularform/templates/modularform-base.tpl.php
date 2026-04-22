<?php if (!empty($sidebar_links)): ?>
  <div class="sidebar">
    <?php print theme('item_list', array('items' => $sidebar_links)); ?>
  </div>
<?php endif; ?>

<div class="content">
  <?php print render($content); ?>
</div>
