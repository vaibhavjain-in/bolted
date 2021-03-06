<?php

/**
 * @file
 * Provides drush commands necessary for site duplication.
 */

use Drupal\acsf\Event\AcsfEvent;

/**
 * Implements hook_drush_command().
 */
function acsf_duplication_drush_command() {
  $items = array();

  $items['acsf-duplication-scrub-batch'] = array(
    'description' => dt('Runs one iteration of the batch scrubbing process on the duplicated site.'),
    'arguments' => array(
      'site_name' => dt('The new name of the duplicated site.'),
      'standard_domain' => dt('The standard domain of the duplicated site.'),
    ),
    'options' => array(
      'exact-copy' => dt('Indicates that the duplicated site is intended to be an exact copy of the source site (i.e. retain all content, users, and required configuration).'),
      'avoid-oom' => dt('The command should run just a single iteration of the batch process (the default is to loop until it runs out of memory). This is useful to keep the memory footprint low but you are expected to handle the looping externally.'),
      'batch' => dt('The number of items to process each iteration (defaults to 1000).'),
      'batch-comment' => dt('The number of comments to delete each iteration (defaults to --batch).'),
      'batch-node' => dt('The number of nodes to delete each iteration (defaults to --batch).'),
      'batch-user' => dt('The number of users to delete each iteration (defaults to --batch).'),
      'retain-content' => dt('Retain nodes and comments (defaults to --exact-copy).'),
      'retain-users' => dt('Retain users (defaults to --exact-copy).'),
    ),
  );
  $items['acsf-duplication-scrub-progress'] = array(
    'description' => dt('Returns status information about the progress of the batch scrubbing process.'),
    'options' => array(
      'format' => array(
        'description' => dt('Format to output the object. Use "var_export" for var_export (default), "print-r" for print_r and "json" for JSON.'),
        'example-value' => 'json',
      ),
    ),
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function acsf_duplication_drush_help($section) {
  switch ($section) {
    case 'drush:acsf-duplication-scrub-batch':
      return dt('Runs one iteration of the batch scrubbing process on the duplicated site.');

    case 'drush:acsf-duplication-scrub-progress':
      return dt('Returns status information about the progress of the batch scrubbing process.');
  }
}

/**
 * Command callback. Scrubs some data on the duplicated site.
 *
 * Runs one iteration of the batch scrubbing process.
 */
function drush_acsf_duplication_scrub_batch($site_name, $standard_domain) {
  if (empty($site_name)) {
    return drush_set_error(dt('You must provide the site name of the duplicated site as the first argument.'));
  }
  if (empty($standard_domain)) {
    return drush_set_error(dt('You must provide the standard domain of the duplicated site as the second argument.'));
  }

  if (!\Drupal::moduleHandler()->moduleExists('acsf')) {
    return drush_set_error(dt('The ACSF module must be enabled.'));
  }

  $context = array(
    'site_name' => $site_name,
    'standard_domain' => $standard_domain,
    'scrub_options' => array(
      'avoid_oom' => drush_get_option('avoid-oom'),
    ),
  );
  \Drupal::moduleHandler()->alter('acsf_duplication_scrub_context', $context);
  ksort($context['scrub_options']);

  // Load and execute the site duplication scrub event handlers.
  $event = AcsfEvent::create('site_duplication_scrub', $context);
  $event->run();

  // Return an error code if the process is incomplete.
  if (\Drupal::state()->get('acsf_duplication_scrub_status', NULL) !== 'complete') {
    return drush_set_error(dt('The scrubbing of this site is incomplete. Please re-run the command to resume processing.'));
  }
  else {
    drush_print(dt('The scrubbing of this site is complete.'));
  }
}

/**
 * Command callback. Outputs progress information on the site duplication scrub.
 */
function drush_acsf_duplication_scrub_progress() {
  if (!\Drupal::moduleHandler()->moduleExists('acsf')) {
    return drush_set_error(dt('The ACSF module must be enabled.'));
  }

  // Get remaining count from the handlers. (Note this code highlights the fact
  // that the countRemaining() method does not return different results
  // depending on whether we pass in a proper event/context; if it does, we'll
  // return bogus results.)
  $empty_event = AcsfEvent::create('site_duplication_scrub');
  $data = array();
  // PHP5.3 compatible syntax for now:
  $handler = new \Drupal\acsf\Event\AcsfDuplicationScrubCommentHandler($empty_event);
  $data['comment_count'] = $handler->countRemaining();
  $handler = new \Drupal\acsf\Event\AcsfDuplicationScrubNodeHandler($empty_event);
  $data['node_count'] = $handler->countRemaining();
  $handler = new \Drupal\acsf\Event\AcsfDuplicationScrubUserHandler($empty_event);
  $data['user_count'] = $handler->countRemaining();

  \Drupal::moduleHandler()->alter('acsf_duplication_scrub_remaining_counts', $data);

  $format = drush_get_option('format', 'var_export');
  drush_print(drush_format($data, NULL, $format));
}
