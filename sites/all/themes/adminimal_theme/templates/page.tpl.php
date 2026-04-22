<?php
/**
 * @file
 * Main page template.
 */
?>


<div class="top-navbar">

  <div class="nav-left">
    <?php if ($logo): ?>
      <a href="<?php print $front_page; ?>" class="nav-logo">
        <img src="<?php print $logo; ?>" alt="Home">
      </a>
    <?php endif;?>
  </div>

  <div class="nav-toggle">&#9776;</div>

  <div class="nav-center">
    <?php
    $main_menu = menu_tree('main-menu');
    print render($main_menu);
    ?>
  </div>

  <div class="nav-right">
    <?php global $user; ?>
    <?php if ($user->uid): ?>
      <span class="nav-user">Hello <?php print check_plain($user->name); ?></span>
      <a href="<?php print url('user/logout', array('query' => array('destination' => 'user/login'))); ?>" class="nav-logout">Logout</a>
    <?php endif; ?>
  </div>

</div>

<div id="page">

  <div id="content" class="clearfix">
    <div class="element-invisible"><a id="main-content"></a></div>

    <?php if ($messages): ?>
      <div id="console" class="clearfix"><?php print $messages; ?></div>
    <?php endif; ?>

    <?php if ($page['help']): ?>
      <div id="help">
        <?php print render($page['help']); ?>
      </div>
    <?php endif; ?>

    <?php if (isset($page['content_before'])): ?>
      <div id="content-before">
        <?php print render($page['content_before']); ?>
      </div>
    <?php endif; ?>

    <?php if ($action_links): ?><ul class="action-links"><?php print render($action_links); ?></ul><?php endif; ?>

    <div id="content-wrapper">

      <?php if (isset($page['sidebar_left'])): ?>
        <div id="sidebar-left">
          <?php print render($page['sidebar_left']); ?>
        </div>
      <?php endif; ?>

      <div id="main-content">
        <?php print render($page['content']); ?>
      </div>

      <?php if (isset($page['sidebar_right'])): ?>
        <div id="sidebar-right">
          <?php print render($page['sidebar_right']); ?>
        </div>
      <?php endif; ?>

    </div>

    <?php if (isset($page['content_after'])): ?>
      <div id="content-after">
        <?php print render($page['content_after']); ?>
      </div>
    <?php endif; ?>

  </div>

  <div id="footer">
    <?php print $feed_icons; ?>
  </div>

</div>
