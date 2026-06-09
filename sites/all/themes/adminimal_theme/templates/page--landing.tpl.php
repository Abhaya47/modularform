<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title><?php print $head_title; ?></title>
  <?php print $head; ?>
  <?php print $styles; ?>
  <?php print $scripts; ?>
  <link href="https://fonts.googleapis.com/css2?family=Sora:wght@300;400;500;600&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    :root {
      --bg: #f7f5f0;
      --surface: #ffffff;
      --border: #e2dfd8;
      --text: #1a1814;
      --muted: #8a877f;
      --accent: #2d5a3d;
      --accent-light: #e8f0ea;
      --accent-hover: #1f4029;
      --radius: 10px;
    }

    body {
      font-family: 'Sora', sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
    }

    /* ── Header ── */
    header {
      background: var(--surface);
      border-bottom: 1px solid var(--border);
      padding: 0 40px;
      height: 60px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      position: sticky;
      top: 0;
      z-index: 50;
    }

    .h-logo {
      font-size: 16px;
      font-weight: 600;
      color: var(--text);
      text-decoration: none;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .h-logo-mark {
      width: 24px; height: 24px;
      background: var(--accent);
      border-radius: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .h-logo-mark svg { display: block; }

    .h-nav {
      display: flex;
      align-items: center;
      gap: 6px;
    }

    .h-nav a {
      padding: 7px 16px;
      border-radius: 8px;
      font-size: 13px;
      font-weight: 500;
      text-decoration: none;
      transition: background 0.15s, color 0.15s;
    }

    .h-nav a.ghost { color: var(--muted); }
    .h-nav a.ghost:hover { background: var(--bg); color: var(--text); }

    .h-nav a.solid {
      background: var(--accent);
      color: #fff;
    }

    .h-nav a.solid:hover { background: var(--accent-hover); }

    /* ── Hero ── */
    .hero {
      flex: 1;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 60px 20px;
      gap: 80px;
    }

    .hero-copy { max-width: 420px; }

    .hero-eyebrow {
      display: inline-block;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.1em;
      text-transform: uppercase;
      color: var(--accent);
      background: var(--accent-light);
      padding: 4px 10px;
      border-radius: 20px;
      margin-bottom: 20px;
    }

    .hero-copy h1 {
      font-size: 40px;
      font-weight: 600;
      line-height: 1.18;
      letter-spacing: -0.8px;
      color: var(--text);
      margin-bottom: 16px;
    }

    .hero-copy p {
      font-size: 15px;
      color: var(--muted);
      line-height: 1.7;
      font-weight: 300;
    }

    /* ── Form card ── */
    .form-card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 16px;
      width: 100%;
      max-width: 380px;
      overflow: hidden;
      flex-shrink: 0;
    }

    /* Tabs */
    .form-tabs {
      display: flex;
      border-bottom: 1px solid var(--border);
    }

    .form-tab {
      flex: 1;
      padding: 14px;
      font-size: 13px;
      font-weight: 500;
      text-align: center;
      cursor: pointer;
      color: var(--muted);
      background: none;
      border: none;
      font-family: 'Sora', sans-serif;
      transition: color 0.15s, background 0.15s;
      border-bottom: 2px solid transparent;
      margin-bottom: -1px;
    }

    .form-tab.active {
      color: var(--accent);
      border-bottom-color: var(--accent);
      background: var(--surface);
    }

    .form-tab:hover:not(.active) { background: var(--bg); }

    /* Panels */
    .form-panel { display: none; padding: 28px; }
    .form-panel.active { display: block; }

    .form-panel h2 {
      font-size: 17px;
      font-weight: 600;
      margin-bottom: 4px;
    }

    .form-panel p {
      font-size: 13px;
      color: var(--muted);
      margin-bottom: 22px;
    }

    /* Fields */
    .field { margin-bottom: 14px; }

    .field label {
      display: block;
      font-size: 12px;
      font-weight: 500;
      color: var(--muted);
      margin-bottom: 6px;
      letter-spacing: 0.02em;
    }

    .field input {
      width: 100%;
      padding: 10px 14px;
      font-family: 'Sora', sans-serif;
      font-size: 14px;
      color: var(--text);
      background: var(--bg);
      border: 1px solid var(--border);
      border-radius: var(--radius);
      outline: none;
      transition: border-color 0.15s, box-shadow 0.15s;
    }

    .field input:focus {
      border-color: var(--accent);
      box-shadow: 0 0 0 3px rgba(45, 90, 61, 0.1);
      background: #fff;
    }

    .field input::placeholder { color: #c5c2bb; }

    /* Submit */
    .btn-submit {
      width: 100%;
      padding: 11px;
      background: var(--accent);
      color: #fff;
      font-family: 'Sora', sans-serif;
      font-size: 14px;
      font-weight: 500;
      border: none;
      border-radius: var(--radius);
      cursor: pointer;
      margin-top: 6px;
      transition: background 0.15s, transform 0.1s;
    }

    .btn-submit:hover { background: var(--accent-hover); }
    .btn-submit:active { transform: scale(0.99); }

    .form-footer {
      text-align: center;
      font-size: 12px;
      color: var(--muted);
      margin-top: 16px;
    }

    .form-footer a { color: var(--accent); text-decoration: none; }
    .form-footer a:hover { text-decoration: underline; }

    /* ── Responsive ── */
    @media (max-width: 800px) {
      .hero { flex-direction: column; gap: 40px; padding: 40px 20px; }
      .hero-copy { text-align: center; }
      .hero-copy h1 { font-size: 30px; }
      header { padding: 0 20px; }
    }
  </style>
</head>
<body>

<?php print $page_top; ?>

<!-- Header -->
<header>
  <a href="<?php print $front_page; ?>" class="h-logo">
    <span class="h-logo-mark">
      <?php if ($logo): ?>
        <img src="<?php print $logo; ?>" alt="" style="height:16px; width:16px; object-fit:contain;">
      <?php else: ?>
        <svg width="14" height="14" viewBox="0 0 14 14" fill="none">
          <path d="M2 7h10M7 2v10" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
        </svg>
      <?php endif; ?>
    </span>
    <?php print check_plain($site_name); ?>
  </a>

</header>

<!-- Hero + Form -->
<section class="hero">

  <div class="hero-copy">
    <span class="hero-eyebrow">Welcome</span>
    <h1><?php print check_plain($site_name); ?></h1>
    <p><?php print $site_slogan ? check_plain($site_slogan) : 'Manage your forms, content and users all in one place. Sign in or create an account to get started.'; ?></p>
  </div>

  <div class="form-card">
    <!-- Tabs -->
    <div class="form-tabs">
      <button class="form-tab active" onclick="switchTab('login', this)">Sign in</button>
      <button class="form-tab" onclick="switchTab('register', this)">Register</button>
    </div>

    <!-- Login panel -->
    <div class="form-panel active" id="panel-login">
      <h2>Welcome back</h2>
      <p>Sign in to your account</p>

      <?php
      $login_form = drupal_get_form('user_login');
      print drupal_render($login_form);
      ?>
      <?php print theme('status_messages'); ?>
    </div>

    <?php print $messages; ?>
    <!-- Register panel -->
    <div class="form-panel" id="panel-register">
      <h2>Create account</h2>
      <p>Free to join, takes 30 seconds</p>

      <?php
      $register_form = drupal_get_form('user_register_form');
      print drupal_render($register_form);
      ?>
    </div>
  </div>

</section>

<?php print $page_bottom; ?>

<script>
  function switchTab(tab, el) {
    document.querySelectorAll('.form-tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.form-panel').forEach(p => p.classList.remove('active'));
    el.classList.add('active');
    document.getElementById('panel-' + tab).classList.add('active');
  }

  // Style Drupal's generated form elements to match our design
  document.querySelectorAll('.form-panel input[type="text"], .form-panel input[type="password"], .form-panel input[type="email"]').forEach(function(el) {
    el.style.width = '100%';
    el.style.padding = '10px 14px';
    el.style.fontFamily = "'Sora', sans-serif";
    el.style.fontSize = '14px';
    el.style.background = 'var(--bg)';
    el.style.border = '1px solid var(--border)';
    el.style.borderRadius = 'var(--radius)';
    el.style.outline = 'none';
    el.style.marginBottom = '14px';
    el.style.boxSizing = 'border-box';
  });

  document.querySelectorAll('.form-panel input[type="submit"]').forEach(function(el) {
    el.style.width = '100%';
    el.style.padding = '11px';
    el.style.background = 'var(--accent)';
    el.style.color = '#fff';
    el.style.fontFamily = "'Sora', sans-serif";
    el.style.fontSize = '14px';
    el.style.fontWeight = '500';
    el.style.border = 'none';
    el.style.borderRadius = 'var(--radius)';
    el.style.cursor = 'pointer';
    el.style.marginTop = '6px';
  });

  document.querySelectorAll('.form-panel label').forEach(function(el) {
    el.style.display = 'block';
    el.style.fontSize = '12px';
    el.style.fontWeight = '500';
    el.style.color = 'var(--muted)';
    el.style.marginBottom = '6px';
  });
</script>

</body>
</html>
