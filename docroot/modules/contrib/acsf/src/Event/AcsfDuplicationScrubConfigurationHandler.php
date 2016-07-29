<?php

/**
 * @file
 * Contains \Drupal\acsf\Event\AcsfDuplicationScrubConfigurationHandler.
 */

namespace Drupal\acsf\Event;

/**
 * Handles the scrubbing of Drupal core state / configuration.
 *
 * At the moment all 'configuration' that is scrubbed, are in fact state
 * variables. Values from the configuration system may be added later, as need
 * arises.
 *
 * Anything that is not specifically core or absolutely required by ACSF should
 * live in gardens_duplication module.
 */
class AcsfDuplicationScrubConfigurationHandler extends AcsfEventHandler {

  /**
   * Implements AcsfEventHandler::handle().
   */
  public function handle() {
    drush_print(dt('Entered @class', array('@class' => get_class($this))));

    // Delete selected state values.
    $variables = array(
      'node.min_max_update_time',
      'system.cron_last',
      'system.private_key',
    );
    $state_storage = \Drupal::state();
    foreach ($variables as $name) {
      $state_storage->delete($name);
    }
  }

}
