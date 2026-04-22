<div class="survey-admin-container">
  <aside class="survey-sidebar">
    <h3></h3>
    <ul>
      <?php foreach ($sidebar_links as $link): ?>
        <li><?php print $link; ?></li>
      <?php endforeach; ?>
    </ul>
  </aside>

  <main class="survey-content">
    <h1>Management Dashboard</h1>
    <div class="action-area">
      <?php print render($content); ?>
    </div>
  </main>
</div>
