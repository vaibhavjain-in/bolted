<?php

/**
 * @file
 * Provides drush commands to set up a site for Acquia Site Factory.
 */

use Drupal\acsf\AcsfInitHtaccessException;
use Drupal\acsf\AcsfSite;
use Drupal\user\Entity\Role;
use Drupal\user\Entity\User;

/**
 * Content that we need to add to customers' .htaccess files.
 */
define('ACSF_HTACCESS_PATCH', '  RewriteCond %{REQUEST_URI} !/sites/g/apc_rebuild.php$');

/**
 * The marker that tells us where we need to add our .htaccess patch.
 */
define('ACSF_HTACCESS_PATCH_MARKER', 'RewriteRule "^(.+/.*|autoload)\.php($|/)" - [F]');

/**
 * A comment to include with the htaccess patch line.
 */
define('ACSF_HTACCESS_PATCH_COMMENT', '  # ACSF requirement: allow access to apc_rebuild.php.');

/**
 * Implements hook_drush_command().
 */
function acsf_init_drush_command() {
  return array(
    'acsf-init' => array(
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
      'callback' => 'drush_acsf_init',
      'description' => dt('Installs/updates the non-standard Drupal components for this repository to be compatible with Acquia Site Factory. This command will update in place, so there is no harm in running it multiple times.'),
      'options' => array(
        'skip-default-settings' => dt('Do not overwrite the default settings.php file. Use this option when installing ACSF in a local environment.'),
      ),
    ),
    'acsf-init-verify' => array(
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
      'callback' => 'drush_acsf_init_verify',
      'description' => dt('Verifies that acsf-init was successfully run in the current version.'),
      'options' => array(
        'skip-default-settings' => dt('Skip verifying the default settings.php file.'),
      ),
    ),
    'acsf-connect-factory' => array(
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
      'callback' => 'drush_acsf_connect_factory',
      'description' => dt('Connect a site to a factory by setting up the right variables in the database.'),
      'aliases' => array('acsf-cf'),
      'options' => array(
        'site-admin-mail' => array(
          'description' => dt('The email address of the Site Factory admin / Gardens admin user. This is typically the "Site Factory admin" user on the factory. These email addresses have to match in order for the initial OpenID connection to bind these accounts.'),
          'required' => TRUE,
          'example-value' => 'user3@example.com',
        ),
        'site-owner-name' => array(
          'description' => dt('The name of the site owner.'),
          'required' => TRUE,
          'example-value' => 'John Smith',
        ),
        'site-owner-mail' => array(
          'description' => dt('The email address of the site owner.'),
          'required' => TRUE,
          'example-value' => 'john.smith@example.com',
        ),
        'site-owner-roles' => array(
          'description' => dt('A list of comma-separated roles (machine names) that should be granted to the site owner (optional).'),
          'example-value' => 'editor, site manager',
        ),
      ),
      'examples' => array(
        'drush acsf-connect-factory --site-admin-mail="user3@example.com" --site-owner-name="John Smith" --site-owner-mail="john.smith@example.com"' => dt('Connect the site to the factory and sets the owner to John Smith.'),
      ),
    ),
    'acsf-uninstall' => array(
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
      'callback' => 'drush_acsf_uninstall',
      'description' => dt('Uninstalls components for this Drupal repository to be compatible with Acquia Site Factory.'),
    ),
  );
}

/**
 * Include exception class files when the autoloader is not available.
 */
function _acsf_init_include_exceptions() {
  $path = dirname(__DIR__) . '/src';
  $classes = array(
    'AcsfException',
    'AcsfInitException',
    'AcsfInitHtaccessException',
  );
  foreach($classes as $class) {
    require_once("$path/$class.php");
  }
}

/**
 * Implements drush_hook_pre_acsf_init().
 */
function drush_acsf_init_pre_acsf_init() {
  _acsf_init_include_exceptions();
}

/**
 * Implements drush_hook_pre_acsf_init_verify().
 */
function drush_acsf_init_pre_acsf_init_verify() {
  _acsf_init_include_exceptions();
}

/**
 * Command callback: executes the required changes to this repository.
 */
function drush_acsf_init() {
  // Retrieve the path to the acsf module relative to this file.
  $path = dirname(dirname(dirname(__FILE__)));
  $relative_path = str_replace(realpath(DRUPAL_ROOT) . '/', '', realpath($path));
  $repo_root = str_replace('/docroot', '', realpath(DRUPAL_ROOT));
  $lib_path = sprintf('%s/%s/acsf/acsf_init/lib', realpath(DRUPAL_ROOT), $relative_path);
  $skip_default_settings = drush_get_option('skip-default-settings');

  drush_print(dt('Installing ACSF requirements.'));

  // Create the required directories.
  foreach (acsf_init_get_required_dirs($repo_root) as $name => $dir) {
    // Skip '../sites/default' if --skip-default-settings is set.
    if ($skip_default_settings && $name == 'site env default') {
      continue;
    }

    drush_print(dt('Creating directory for !name at !dir', array(
      '!name' => $name,
      '!dir' => $dir,
    )));
    if (!file_exists($dir)) {
      if (mkdir($dir, 0755, TRUE)) {
        drush_log(dt('Success'), 'success');
      }
      else {
        drush_log(dt('Error'), 'error');
      }
    }
    else {
      drush_log(dt('Already Exists'), 'ok');
    }
  }

  // Copy the required files.
  foreach (acsf_init_get_required_files($repo_root) as $location) {
    $file = $location['filename'];
    // Skip the default settings.php file if --skip-default-settings is set.
    if ($skip_default_settings && $file == 'settings.php' && $location['source'] == 'sites/default') {
      continue;
    }

    $dest = sprintf('%s/%s', $location['dest'], $file);

    // Some files only contain a destination as they are already in place.
    if (isset($location['source']) && isset($location['dest'])) {
      $source = sprintf('%s/%s/%s', $lib_path, $location['source'], $file);
      drush_print(dt('Copying !file to !dest.', array(
        '!file' => $source,
        '!dest' => $dest,
      )));
      if (file_exists($dest)) {
        $confirm = drush_confirm(dt('Destination file exists, continue?'));
        if ($confirm === FALSE) {
          continue;
        }
      }
      // Copy the file into the destination.
      if (copy($source, $dest)) {
        drush_log(dt('Copy Success: !file', array('!file' => $file)), 'success');
      }
      else {
        drush_log(dt('Copy Error: !file', array('!file' => $file)), 'error');
      }
      // If the file exists, it could be set to 0444, so we have to ensure that
      // it is writable before overwriting it. The copy would fail otherwise.
      if (!is_writable($dest)) {
        if (!chmod($dest, 0666)) {
          drush_log(dt('Chmod Error: !file', array('!file' => $file)), 'error');
        };
      }
    }

    // Chmod the file if required.
    $mod = isset($location['mod']) ? $location['mod'] : FALSE;
    if ($mod && chmod($dest, $mod)) {
      drush_log(dt('Chmod Success: !file', array('!file' => $file)), 'success');
    }
    elseif ($mod) {
      drush_log(dt('Chmod Error: !file', array('!file' => $file)), 'error');
    }
  }

  try {
    drush_acsf_init_patch_htaccess();
  }
  catch (AcsfInitHtaccessException $e) {
    drush_log($e->getMessage(), 'error');
  }

  // Verify that the files are in sync.
  clearstatcache();
  $result = drush_acsf_init_verify();

  if ($result) {
    drush_print(dt('Be sure to commit any changes to your repository before deploying.'));
  }
}

/**
 * Command callback: Verify that acsf-init was run against the current version.
 */
function drush_acsf_init_verify() {
  $drupal_root = rtrim(DRUPAL_ROOT, '/.');
  // Retrieve the path to the acsf module relative to this file.
  $path = dirname(dirname(dirname(__FILE__)));
  $relative_path = str_replace($drupal_root . '/', '', $path);
  $repo_root = str_replace('/docroot', '', $drupal_root);
  $lib_path = sprintf('%s/%s/acsf/acsf_init/lib', $drupal_root, $relative_path);
  $skip_default_settings = drush_get_option('skip-default-settings');

  $error = FALSE;
  foreach (acsf_init_get_required_files($repo_root) as $location) {
    $file = $location['filename'];
    // Skip the default settings.php file if --skip-default-settings is set.
    if ($skip_default_settings && $file == 'settings.php' && $location['source'] == 'sites/default') {
      continue;
    }

    $dest = sprintf('%s/%s', $location['dest'], $file);

    // Some files only contain a destination as they are already in place.
    if (isset($location['source']) && isset($location['dest'])) {
      $source = sprintf('%s/%s/%s', $lib_path, $location['source'], $file);
      if (!file_exists($dest)) {
        $error = TRUE;
        drush_set_error('FILE MISSING', dt('The file !file is missing.', array(
          '!file' => $file,
        )));
      }
      elseif (md5_file($source) != md5_file($dest)) {
        $error = TRUE;
        drush_set_error('FILE OUT OF DATE', dt('The file !file is out of date.', array(
          '!file' => $file,
        )));
      }
    }

    // Verify the file is executable.
    // Note: The approach here is to not check for the exact file perms (in
    // other words to not test against the 'mod' element), since git - by
    // design - does not respect anything beyond a simple executable bit, the
    // other perms may be filesystem/OS-dependent, and can't be guaranteed to be
    // consistent.
    if (file_exists($dest) && !empty($location['test_executable'])) {
      $dest_permissions = fileperms($dest);
      // We do want to test the owner executable bit, and the group executable
      // bit as well.
      // e.g. to test whether the owner has execute permission, it is the case
      // of testing with: 00000000 01000000 (which is 0100 in octal, 64 in
      // decimal).
      if (($dest_permissions & (0100 | 0010)) != (0100 | 0010)) {
        $error = TRUE;
        drush_set_error('INCORRECT FILE MODE', dt('The file !file is not executable. Make this file executable for the owner and group, then commit it again.', array(
          '!file' => $file,
        )));
      }
    }
  }

  if (!drush_acsf_init_test_htaccess_is_patched()) {
    drush_set_error('HTACCESS UNPATCHED', dt('The .htaccess file has not been patched to allow access to apc_rebuild.php.'));
  }

  if ($error) {
    drush_print(dt('Please run drush acsf-init to correct these problems and commit the resulting code changes.'));
    return FALSE;
  }
  else {
    // The Site Factory code deployment uses this string to determine if the
    // acsf-init has been properly run. If this is changed, also ensure that
    // the check in VcsVerifyAcsf matches.
    drush_log(dt('acsf-init required files ok'), 'success');
    return TRUE;
  }
}

/**
 * Command callback: Connect a site to a Factory.
 */
function drush_acsf_connect_factory() {
  // Preliminary validation before starting to modify the database.
  $site_admin_mail = trim(drush_get_option('site-admin-mail'));
  $site_owner_name = trim(drush_get_option('site-owner-name'));
  $site_owner_mail = trim(drush_get_option('site-owner-mail'));
  $site_owner_roles = drush_get_option_list('site-owner-roles');

  // Validate email addresses.
  $validator = \Drupal::service('email.validator');
  if (!$validator->isValid($site_admin_mail)) {
    return drush_set_error(dt('The site-admin-mail value is not a valid email address.'));
  }
  if (!$validator->isValid($site_owner_mail)) {
    return drush_set_error(dt('The site-owner-mail value is not a valid email address.'));
  }

  $user_storage = \Drupal::entityManager()->getStorage('user');

  // Make sure there is no regular user account with the admin email address.
  $site_admin_mail_accounts = $user_storage->loadByProperties(array(
    'mail' => $site_admin_mail
  ));
  $site_admin_mail_account = reset($site_admin_mail_accounts);
  if ($site_admin_mail_account && $site_admin_mail_account->id() > 1) {
    return drush_set_error(dt('Unable to sync the admin account, the email address @mail is already used by the user account @uid.', array(
      '@mail' => $site_admin_mail,
      '@uid' => $site_admin_mail_account->id(),
    )));
  }

  // The site owner's email address may have been changed on the factory (for
  // instance, if the user updated their email address on the factory and the
  // new email address has not yet been synced to the site). First, try to
  // locate the user account by site-owner-mail.
  $site_owner_accounts = $user_storage->loadByProperties(array(
    'mail' => $site_owner_mail
  ));
  $site_owner_account = reset($site_owner_accounts);
  if ($site_owner_account && $site_owner_account->getUsername() !== $site_owner_name) {
    return drush_set_error(dt('The site-owner-name value does not match the name of the user loaded by site-owner-mail.'));
  }
  // If the site owner user account is not found, try to locate it by
  // site-owner-name.
  if (!$site_owner_account) {
    $site_owner_accounts = $user_storage->loadByProperties(array(
      'name' => $site_owner_name
    ));
    $site_owner_account = reset($site_owner_accounts);
  }
  // If the site owner account is still not found then either the customer has
  // made a typo or maybe there is going to be a new owner who needs a new
  // account. Ask for confirmation to create a new account.
  if (!$site_owner_account) {
    if (!drush_confirm(dt('The site owner name or email address that you provided does not correspond to any account on the site. Do you want to create a new account?'))) {
      return drush_user_abort();
    }
  }

  // Clear all caches ahead of time so Drupal has a chance to rebuild
  // registries.
  drupal_flush_all_caches();
  acsf_build_registry();
  drush_log(dt('Cleared all caches.'), 'ok');

  // Set default settings for user accounts.

  $admin_role_ids = \Drupal::EntityQuery('user_role')
    ->condition('is_admin', TRUE)
    ->execute();

  // Take over uid 1 with our Site Factory admin user.
  $admin_account = User::load(1);
  // Create a new user if uid 1 doesn't exist.
  if (!$admin_account) {
    $admin_account = User::create(array('uid' => 1));
  }
  // Ensure the default admin role is added to the account.
  $admin_account->addRole(reset($admin_role_ids));
  // Set login time to avoid e-mail verification needed error.
  $admin_account->setLastLoginTime(1)
    ->setUsername('Site Factory admin')
    ->setEmail($site_admin_mail)
    ->setPassword(user_password())
    ->activate()
    ->save();

  // Create or update site owner account.
  // Prepare roles for site owner.
  if (!$site_owner_account) {
    $site_owner_account = User::create();
  }
  foreach ($site_owner_roles as $owner_role) {
    if (Role::load($owner_role)) {
      $site_owner_account->addRole($owner_role);
    }
    else {
      drush_log(dt('The role @role does not exist; not adding it to the site owner.', array('@role' => $owner_role)), 'warning');
    }
  }
  // Site owners also get the default administrator role.
  $site_owner_account->addRole(reset($admin_role_ids));

  $site_owner_account->setLastLoginTime(1)
    ->setUsername($site_owner_name)
    ->setEmail($site_owner_mail)
    ->setPassword(user_password())
    ->activate()
    ->save();

  drush_log(dt('Synched Site Factory admin and site owner accounts.'), 'ok');

  // Remove acsf variable so that it can be repopulated with the right value
  // on the next acsf-site-sync.
  \Drupal::service('acsf.variable_storage')->delete('acsf_site_info');

  // Reset the local site data and run acsf-site-sync to fetch factory data
  // about the site.
  $site = AcsfSite::load();
  $site->clean();
  drush_acsf_site_sync();
  drush_log(dt('Executed acsf-site-sync to gather site data from factory and reset all acsf variables.'), 'ok');

  // Clear all caches.
  drupal_flush_all_caches();
  drush_log(dt('Cleared all caches.'), 'ok');

  // Send a theme event notification to the Factory.
  \Drupal::service('acsf.theme_notification')->sendNotification('site', 'create');
}

/**
 * Command callback: Uninstalls ACSF files from the code base.
 */
function drush_acsf_uninstall() {
  drush_print('Removing ACSF requirements.');

  $repo_root = str_replace('/docroot', '', DRUPAL_ROOT);
  foreach (acsf_init_get_required_files($repo_root) as $location) {
    $file = $location['filename'];
    $dest = sprintf('%s/%s', $location['dest'], $file);

    // Some files only contain a destination as they are already in place.
    if (isset($location['src']) && file_exists($dest)) {
      $confirm = drush_confirm(dt('Delete !file?', array('!file' => $dest)));
      if ($confirm === FALSE) {
        continue;
      }
      if (unlink($dest)) {
        drush_log(dt('Success'), 'success');
      }
      else {
        drush_log(dt('Error'), 'error');
      }
    }
  }
}

/**
 * Lists all required directories to create.
 */
function acsf_init_get_required_dirs($repo_root) {
  return array(
    'cloud hooks' => sprintf('%s/hooks/common/post-db-copy', $repo_root),
    'cloud hook deploy' => sprintf('%s/hooks/common/pre-web-activate', $repo_root),
    'acquia hook dir' => sprintf('%s/hooks/acquia', $repo_root),
    'cloud hook samples' => sprintf('%s/hooks/samples', $repo_root),
    'site config logic' => sprintf('%s/sites/g', DRUPAL_ROOT),
    'site env default' => sprintf('%s/sites/default', DRUPAL_ROOT),
  );
}

/**
 * Lists all required files to create/delete.
 */
function acsf_init_get_required_files($repo_root) {
  // Path to the ACSF module file.
  $acsf_path = dirname(__DIR__);

  // Array elements should use the following guidelines:
  // - Use the 'source' element to indicate where the file should be copied
  //   from. Note: Some files do not have a source as they are already in place.
  // - Use the 'dest' element to specify where the file will be copied to.
  // - Use the 'mod' element to describe an octal for the file permissions -
  //   must be chmod() compatible. e.g. 0755
  // - Use the 'test_executable' element to enforce testing executability of the
  //   file. "Executable" files are expected to be owner and group executable.

  return array(
    array(
      'filename' => 'README.md',
      'source' => 'cloud_hooks',
      'dest' => sprintf('%s/hooks', $repo_root),
    ),
    array(
      'filename' => '000-acquia_required_scrub.php',
      'source' => 'cloud_hooks/common/post-db-copy',
      'dest' => sprintf('%s/hooks/common/post-db-copy', $repo_root),
      'mod' => 0750,
      'test_executable' => TRUE,
    ),
    array(
      'filename' => '000-acquia-deployment.php',
      'source' => 'cloud_hooks/common/pre-web-activate',
      'dest' => sprintf('%s/hooks/common/pre-web-activate', $repo_root),
      'mod' => 0750,
      'test_executable' => TRUE,
    ),
    array(
      'filename' => 'db_connect.php',
      'source' => 'cloud_hooks/acquia',
      'dest' => sprintf('%s/hooks/acquia', $repo_root),
    ),
    array(
      'filename' => 'uri.php',
      'source' => 'cloud_hooks/acquia',
      'dest' => sprintf('%s/hooks/acquia', $repo_root),
    ),
    array(
      'filename' => 'acquia-cloud-site-factory-post-db.sh',
      'source' => 'cloud_hooks/samples',
      'dest' => sprintf('%s/hooks/samples', $repo_root),
    ),
    array(
      'filename' => 'hello-world.sh',
      'source' => 'cloud_hooks/samples',
      'dest' => sprintf('%s/hooks/samples', $repo_root),
    ),
    array(
      'filename' => 'post-code-deploy.tmpl',
      'source' => 'cloud_hooks/samples',
      'dest' => sprintf('%s/hooks/samples', $repo_root),
    ),
    array(
      'filename' => 'sites.php',
      'source' => 'sites',
      'dest' => sprintf('%s/sites', DRUPAL_ROOT),
    ),
    array(
      'filename' => 'apc_rebuild.php',
      'source' => 'sites/g',
      'dest' => sprintf('%s/sites/g', DRUPAL_ROOT),
    ),
    array(
      'filename' => '.gitignore',
      'source' => 'sites/g',
      'dest' => sprintf('%s/sites/g', DRUPAL_ROOT),
    ),
    array(
      'filename' => 'services.yml',
      'source' => 'sites/g',
      'dest' => sprintf('%s/sites/g', DRUPAL_ROOT),
    ),
    array(
      'filename' => 'settings.php',
      'source' => 'sites/g',
      'dest' => sprintf('%s/sites/g', DRUPAL_ROOT),
    ),
    array(
      'filename' => 'SimpleRest.php',
      'source' => 'sites/g',
      'dest' => sprintf('%s/sites/g', DRUPAL_ROOT),
    ),
    array(
      'filename' => 'sites.inc',
      'source' => 'sites/g',
      'dest' => sprintf('%s/sites/g', DRUPAL_ROOT),
    ),
    array(
      'filename' => 'settings.php',
      'source' => 'sites/default',
      'dest' => sprintf('%s/sites/default', DRUPAL_ROOT),
      'mod' => 0644,
    ),
    array(
      'filename' => 'acsf_cron',
      'dest' => $acsf_path . '/scripts',
      'mod' => 0550,
      'test_executable' => TRUE,
    ),
    array(
      'filename' => 'acsf_ssh',
      'dest' => $acsf_path . '/scripts',
      'mod' => 0550,
      'test_executable' => TRUE,
    ),
  );
}

/**
 * Patches the htaccess file to allow access to our apc_rebuild.php script.
 *
 * We do not use the unix patch utility here, as it might be too fussy about
 * line numbers and context.  We want to ensure that our line is present in the
 * .htaccess file in an appropriate place, without caring too much about the
 * content of surrounding lines.  There will be extreme cases where this
 * patching can fail if modifications to the customer's .htaccess file cause it
 * to be wildly different to stock Drupal 8.  In this case, they can manually
 * add the line, and we will accept that manual modification.  This doesn't
 * seem like a situation we need to handle until we have at least one case.  If
 * a customer was for some reason really sure that they did not want our line in
 * their .htaccess file, then adding it verbatim, but commented-out would
 * suffice.
 */
function drush_acsf_init_patch_htaccess() {
  if (drush_acsf_init_test_htaccess_is_patched()) {
    return;
  }

  drush_print(dt('Patching .htaccess file.'));
  // All the lines we've scanned so far up until the marker line.
  $output_lines = array();
  // The output as a string.
  $output = '';
  // Iterate line-by-line until we reach the line which our rule MUST precede...
  $fp = fopen(drush_acsf_init_get_htaccess_path(), 'r+');
  $marker_found = FALSE;
  while (($line = fgets($fp, 4096)) !== FALSE) {
    $output_lines[] = $line;

    if (strpos($line, ACSF_HTACCESS_PATCH_MARKER) !== FALSE) {
      $marker_found = TRUE;
      // ... And then backtrack all the lines of comments that precede that
      // marker line.
      $marker_index = count($output_lines) - 1;
      while ($marker_index > 0 && preg_match('/^\s*#.*$/', $output_lines[$marker_index - 1])) {
        $marker_index--;
      }
      if ($marker_index == 0) {
        // This should never happen - it could only happen if the entire
        // .htaccess file preceding the marker line were all comments.  We
        // cannot consider this valid, as the minimum requirements for
        // rewrites are not met.  We'd expect at a minimum
        // <IfModule mod_rewrite.c> ... RewriteEngine on
        throw new AcsfInitHtaccessException('Reached the beginning of the file but was unable to find a place to insert the .htaccess patch.  The .htaccess file can be manually handled to fix this error.');
      }

      // Insert our patch with preceding comment, before the marker index,
      // i.e. after the first line seen which is not a comment.
      array_splice($output_lines, $marker_index, 0, array(
        ACSF_HTACCESS_PATCH_COMMENT . "\n",
        ACSF_HTACCESS_PATCH . "\n"
      ));
      $output = implode('', $output_lines);

      break;
    }
  }

  if (!$marker_found) {
    // We were unable to locate the marker that disallows access to PHP files -
    // it may have been modified which makes it impossible to locate.
    // Alternatively, it may have been removed entirely, which actually means we
    // don't need to patch the file at all. @see
    // drush_acsf_init_patch_htaccess() for info on how to convince the
    // verification to accept the file.
    throw new AcsfInitHtaccessException('Unable to locate the marker for patching the .htaccess file. This file will need manual patching to allow access to the apc_rebuild.php file.');
  }

  // Then append the rest of the file.
  while (($line = fgets($fp, 4096)) !== FALSE) {
    $output .= "$line";
  }

  file_put_contents(drush_acsf_init_get_htaccess_path(), $output);

  if (drush_acsf_init_test_htaccess_is_patched()) {
    drush_log(dt('Successfully patched .htaccess file.'), 'ok');
  }
  else {
    drush_log(dt('Failed to patch .htaccess file.'), 'error');
  }
}

/**
 * Determines whether htaccess allows access to our apc_rebuild.php script.
 */
function drush_acsf_init_test_htaccess_is_patched() {
  if (!file_exists(drush_acsf_init_get_htaccess_path()) || !($content = file_get_contents(drush_acsf_init_get_htaccess_path()))) {
    // If the htaccess file does not exist or is empty, then it cannot forbid
    // access to apc_rebuild.php, so we can consider this a pass, albeit a weird
    // one.
    return TRUE;
  }

  return strpos($content, ACSF_HTACCESS_PATCH) !== FALSE;
}

/**
 * Returns the path to the .htaccess file within the codebase.
 *
 * @return string
 *   The full absolute path to the .htaccess file.
 */
function drush_acsf_init_get_htaccess_path() {
  return DRUPAL_ROOT . '/.htaccess';
}
