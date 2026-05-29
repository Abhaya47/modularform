<?php
/**
 * @file modularform-people-list.tpl.php
 * Variables:
 *   $people — array of ['label', 'value', 'role']
 */
?>
<div class="mf-plist-wrap">

  <div class="mf-plist-header">
    <div class="mf-plist-search-wrap">
      <span class="mf-plist-search-icon">&#9906;</span>
      <input
        type="text"
        class="mf-plist-search"
        placeholder="<?php print t('Search members...'); ?>"
        autocomplete="off"
      />
      <span class="mf-plist-count" id="mf-plist-count">
        <?php print count($people); ?> <?php print t('people'); ?>
      </span>
    </div>
  </div>

  <ul class="modularform-people-existing mf-plist" id="modularform-people-existing">
    <?php foreach ($people as $person): ?>
      <?php $role = isset($person['role']) ? (int) $person['role'] : 2; ?>
      <li class="mf-plist-item" data-value="<?php print check_plain($person['value']); ?>"
          data-label="<?php print strtolower(check_plain($person['label'])); ?>">

        <div class="mf-plist-avatar">
          <?php print strtoupper(substr(check_plain($person['label']), 0, 1)); ?>
        </div>

        <span class="mf-plist-label">
          <?php print check_plain($person['label']); ?>
        </span>

        <select class="mf-plist-role modularform-people-role"
                data-for="<?php print check_plain($person['value']); ?>">
          <option value="2" <?php print $role === 2 ? 'selected' : ''; ?>>
            <?php print t('Participant'); ?>
          </option>
          <option value="1" <?php print $role === 1 ? 'selected' : ''; ?>>
            <?php print t('Viewer'); ?>
          </option>
          <option value="0" <?php print $role === 0 ? 'selected' : ''; ?>>
            <?php print t('Creator'); ?>
          </option>
        </select>

        <button type="button" class="mf-plist-delete modularform-people-delete"
                title="<?php print t('Remove'); ?>" aria-label="<?php print t('Remove'); ?>">
          &times;
        </button>

      </li>
    <?php endforeach; ?>
  </ul>

  <div class="mf-plist-empty" id="mf-plist-empty" style="display:none;">
    <?php print t('No members match your search.'); ?>
  </div>

</div>
