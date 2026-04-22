<div class="landing-wrapper">

  <header class="landing-header">
    <h1><?php print $message; ?></h1>
  </header>

  <section class="landing-content">
    <p>This is your custom Drupal landing page.</p>

    <a href="<?php print url('user/login'); ?>" class="btn-login">
      Login
    </a>

    <a href="<?php print url('user/register'); ?>" class="btn-register">
      Register
    </a>
  </section>

</div>
