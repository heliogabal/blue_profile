<?php

/**
 * @file
 * Drush extension for integrating with the drupal.org packaging system.
 *
 * This extension serves the following purposes:
 *  - Adds the --drupal-org and --build-run-key command line options,
 *    which activate the functionality in the rest of the code.
 *  - Validates .make files to ensure they comply with drupal.org packaging
 *    requirements.
 *  - Restricts download locations to those approved for drupal.org packages.
 *  - Implements a custom logging function for error reporting to the packaging
 *    script.
 *  - Builds a list of package contents (nids of package items), and stores
 *    for the packaging system to use.
 */

// URI of 'Packaging instructions on drupal.org' handbook page.
define('DRUPALORG_DRUSH_DOCUMENTATION_LINK', 'http://drupal.org/node/1432190');
// URL of 'Packaging library whitelist' page on drupal.org.
define('DRUPALORG_DRUSH_PACKAGING_LIBRARY_WHITELIST', 'http://drupal.org/packaging-whitelist');
// URL of allowed patches on drupal.org.
define('DRUPALORG_DRUSH_ALLOWED_PATCH_URL', '#^https?://(www\.)?drupal.org/files#');
// TODO: Rip this out after JSON format is deployed.
// Name of the package contents .txt file used by the packaging script.
define('DRUPALORG_DRUSH_PACKAGE_CONTENTS_FILE', 'package_contents.txt');
// Name of the package contents JSON file used by the packaging script.
define('DRUPALORG_DRUSH_PACKAGE_CONTENTS_JSON_FILE', 'package_contents.json');
// Name of the build errors file used by the packaging script.
define('DRUPALORG_DRUSH_BUILD_ERRORS_FILE', 'build_errors.txt');

/**
 * Implement hook_drush_command().
 */
function drupalorg_drush_drush_command() {
  $items = array();
  $items['verify-makefile'] = array(
    'description' => "Verify the specified makefile is in a drupal.org friendly format. Accepts all 'make' command options, non-applicable options are no-ops.",
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Implements hook_drush_help_alter().
 */
function drupalorg_drush_drush_help_alter(&$command) {
  // Add Drupal.org-specific options to the `make` command.
  if ($command['command'] == 'make') {
    $command['options'] = array_merge($command['options'], drupalorg_drush_make_options());
  }
  // Add Drupal.org-specific options to the `verify-makefile` command.
  // Overload the options for the `make` command. This is done because:
  //  - The commands use some make command options internally.
  //  - Allows them to be run with the exact same options as the make command,
  //    especially useful for the testing harness.
  elseif ($command['command'] == 'verify-makefile') {
    $command_options = make_drush_command();
    $command['options'] = array_merge(drupalorg_drush_make_options(), $command_options['make']['options']);
  }
}

/**
 * Common options for the drupalorg_drush make plugin.
 */
function drupalorg_drush_make_options() {
  return array(
    'drupal-org' => "Use Drupal.org validation rules. Can be either 'contrib' (the default if you don't specify a value) or 'core'.",
    'drupal-org-project' => 'Project short name being built.',
    'drupal-org-log-errors-to-file' => 'Log make errors to a file',
    // TODO: Rip this out after JSON format is deployed.
    'drupal-org-log-package-items-to-file' => 'Log package items to a file',
    'drupal-org-log-package-metadata' => 'Log package item metadata to a JSON file',
    'drupal-org-build-root' => 'Location to place custom build files',
    'drupal-org-whitelist-url' => 'URL of the human-readable packaging library whitelist',
    'drupal-org-allowed-patch-url' => 'A regular expression for URLs of allowed patches in .make files',
  );
}

/**
 * Drush callback; verify a .make file is in a drupal.org friendly format.
 */
function drush_drupalorg_drush_verify_makefile($makefile = '') {
  // If --version option is supplied, print make's version and bail.
  if (drush_get_option('version', FALSE)) {
    return drush_make();
  }

  // Figure out what file(s) to process and what validation rules to use.
  $makefiles = array();
  if (!empty($makefile)) {
    $makefiles[$makefile] = drush_get_option('drupal-org', 'contrib');
  }
  else {
    if (file_exists('drupal-org.make')) {
      $makefiles['drupal-org.make'] = 'contrib';
    }
    if (file_exists('drupal-org-core.make')) {
      $makefiles['drupal-org-core.make'] = 'core';
    }
  }
  if (empty($makefiles)) {
    make_error('FILE_ERROR', dt('You must either specify a .make filename as an argument or have a drupal-org.make and/or drupal-org-core.make file in the current directory.'));
    return FALSE;
  }

  $core_version = NULL;
  foreach ($makefiles as $filename => $validation_type) {
    drush_set_option('drupal-org', $validation_type);
    $info = make_validate_info_file(make_parse_info_file($filename));
    if (empty($core_version)) {
      $core_version = $info['core'];
    }
    if ($core_version != $info['core']) {
      make_error('BUILD_ERROR', dt("The 'core' attribute must be the same in both drupal-org.make and drupal-org-core.make."));
    }
    $errors = drush_get_error();
    if (empty($errors)) {
      drush_log(dt('Makefile @makefile passed.', array('@makefile' => $filename)), 'ok');
    }
  }
}


/**
 * Implement EXTENSION_drush_init().
 */
function drupalorg_drush_drush_init() {
  $release_info = drush_get_option('release-info', 'updatexml');
  drush_include_engine('release_info', $release_info);
  $drupal_org = drush_get_option('drupal-org');
  if (!empty($drupal_org)) {
    // The --drupal-org switch implies these defaults:
    // Location to put our custom build files.
    drush_set_default('drupal-org-build-root', '.');
    // The destination of the downloaded contrib projects.
    drush_set_default('contrib-destination', '.');
    if ($drupal_org !== 'core') {
      // Whether to allow a build without core.
      drush_set_default('no-core', TRUE);
    }

    // Optionally set up a custom error logger.
    if (drush_get_option('drupal-org-log-errors-to-file')) {
      drush_set_context('DRUSH_LOG_CALLBACK', 'drupalorg_drush_log_errors_to_file');
    }

    // Optionally set up the package metadata file. The packaging script
    // expects it, so it's created as an empty file here to ensure it exists.
    $metadata_file = drupalorg_drush_metadata_file();
    if (!empty($metadata_file)) {
      if (!touch($metadata_file)) {
        make_error('FILE_ERROR', dt('Unable to write package metadata file: %metadata_file', array('%metadata_file' => $metadata_file)));
      }
    }
    // TODO: Rip this out after JSON format is deployed.
    if (drush_get_option('drupal-org-log-package-items-to-file')) {
      $drupal_org_build_root = drush_get_option('drupal-org-build-root');
      if (!touch($drupal_org_build_root . '/' . DRUPALORG_DRUSH_PACKAGE_CONTENTS_FILE)) {
        make_error('FILE_ERROR', dt('Unable to write package contents file to %build_root', array('%build_root' => $drupal_org_build_root)));
      }
    }
  }
}

/**
 * Implement EXTENSION_make_validate_info().
 */
function drupalorg_drush_make_validate_info($info) {
  $drupal_org = drush_get_option('drupal-org');
  if (!empty($drupal_org)) {
    if ($drupal_org !== 'core') {
      $drupal_org = 'contrib';
    }
    drush_log(dt("Starting Drupal.org %type makefile validation, please wait", array('%type' => $drupal_org)), 'ok');
    // First, setup the transformers that are shared between core and contrib.
    $transformers = array(
      'top' => array(
        'DrushMakeDo_TopWhitelist',
      ),
      'project' => array(
        'DrushMakeDo_ProjectType',
        'DrushMakeDo_DownloadWhitelist',
        'DrushMakeDo_DownloadRevision',
        'DrushMakeDo_DownloadGit',
        'DrushMakeDo_DownloadURL',
        'DrushMakeDo_ProjectPatch',
      ),
    );
    // Now, add some special-case transformers depending on core or contrib.
    // The allowed project-level attributes are (slightly) different between
    // core and contrib, and given our existing class plubming, it's easier to
    // just define separate validators for this.
    if ($drupal_org === 'core') {
      $transformers['project'][] = 'DrushMakeDo_CoreProjectWhitelist';
    }
    else {
      $transformers['project'][] = 'DrushMakeDo_ContribProjectWhitelist';
    }

    $pass = TRUE;
    foreach ($transformers['top'] as $transformer) {
      drush_log(dt("Running transformer !transformer on .make file", array('!transformer' => $transformer)), 'debug');
      $object = new $transformer('make');
      if (!$object->verify($info)) {
        $pass = FALSE;
      }
    }

    if (!empty($info['projects'])) {
      foreach ($info['projects'] as $project => $project_data) {
        if ($project === drush_get_option('drupal-org-project', NULL)) {
          make_error('BUILD_ERROR', dt("Project %project can not be included in itself.", array('%project' => $project)));
          $pass = FALSE;
          // No point doing the per-project validation on these.
          continue;
        }
        if ($drupal_org === 'core') {
          if ($project != 'drupal') {
            make_error('BUILD_ERROR', dt("Project %project is not allowed for a core-only .make file.", array('%project' => $project)));
            $pass = FALSE;
            // No point doing the per-project validation on these.
            continue;
          }
        }
        else {
          if ($project == 'drupal') {
            make_error('BUILD_ERROR', dt("projects[drupal] is not allowed in a regular drupal-org.make file. Use the 'core' attribute to specify a stable release of Drupal core. For -dev releases and patches, use a drupal-org-core.make file."));
            $pass = FALSE;
            // No point doing the per-project validation on this.
            continue;
          }
        }
        foreach ($transformers['project'] as $transformer) {
          drush_log(dt("Running transformer !transformer on .make file for project !project", array('!transformer' => $transformer, '!project' => $project)), 'debug');
          $object = new $transformer('make');
          if (!$object->verify($project_data, $project, $info['core'])) {
            $pass = FALSE;
          }
        }
        // Record metadata about this project (if we care).
        drupalorg_drush_record_project_metadata($project, $project_data, $info['core']);
        // Save all the changes back into the $info array for this project.
        $info['projects'][$project] = $project_data;
      }
    }

    if (!empty($info['libraries'])) {
      if ($drupal_org === 'core') {
        make_error('BUILD_ERROR', dt('Defining libraries in a drupal-org-core.make file is not permitted.'));
        $pass = FALSE;
      }
      else {
        $library_transformer = new DrushMakeDo_LibraryWhitelist('make');
        if ($library_transformer->whitelist_loaded()) {
          foreach ($info['libraries'] as $library => $library_data) {
            drush_log(dt("Running transformer DrushMakeDo_LibraryWhitelist on .make file for library !library", array('!library' => $library)), 'debug');
            if ($library_transformer->verify($library_data, $library)) {
              if (drupalorg_drush_metadata_file()) {
                drupalorg_drush_metadata('library', $library, drupalorg_drush_get_library_metadata($library_data));
              }
            }
            else {
              $pass = FALSE;
            }
            $info['libraries'][$library] = $library_data;
          }
        }
        else {
          $pass = FALSE;
        }
      }
    }

    // Abort if any errors were found.
    if (!$pass) {
      make_error('BUILD_ERROR', dt('The Drupal.org validation check failed -- see @doc_link for more information.', array('@doc_link' => DRUPALORG_DRUSH_DOCUMENTATION_LINK)));
      return FALSE;
    }
    elseif (drupalorg_drush_metadata_file() && !drupalorg_drush_write_package_metadata()) {
      return FALSE;
    }
  }
  return $info;
}

abstract class DrushMakeDo_Transformer {
  // TODO: This function should be better named, (error_log maybe?), or should
  // go away in favor of calling make_error() directly.
  protected function log($message) {
    make_error('BUILD_ERROR', $message);
  }

  /**
    TODO: http://drupal.org/node/1371312
  protected function latestVersion($project_name, $version, $core) {

    // Project types that are valid for the version checker.
    $allowed_project_types = array(
      'core',
      'module',
      'theme',
    );
    $project = array(
      'name'           => $project_name,
      'core'           => $core,
      'version'        => $version,
      'location'       => drush_get_option('make-update-default-url', RELEASE_INFO_DEFAULT_URL),
    );

    $request = make_prepare_request($project);
    $release = release_info_fetch($request);

    // Because the version data we get back from the XML may not be the same
    // data we passed, and because we can fail even if version data is
    // returned, track both the success/failure of the request, and  the
    // version data.

    // Check for bad projects and branch releases.
    if (empty($release) || preg_match('/.*-dev$/', $release['version'])) {
      // Use the passed version if no version was returned.
      if (empty($release)) {
        $final_version = $version;
      }
      else {
        $final_version = $release['version'];
      }
      if ($project_name == 'drupal') {
        make_error('BUILD_ERROR', dt("Drupal core does not have an official release for version '%version' yet.", array('%version' => $final_version)));
      }
      else {
        make_error('BUILD_ERROR', dt("Project '%project' does not have an official release for version '%version'.", array('%project' => $project_name, '%version' => $final_version)));
      }
      $result = FALSE;
    }
    // Make sure the project is an allowed project type.
    elseif (!in_array($release['type'], $allowed_project_types)) {
      make_error('BUILD_ERROR', dt("Project %project of type '%type' is not permitted.", array('%project' => $project_name, '%type' => $release['type'])));
      $final_version = $release['version'];
      $result = FALSE;
    }
    else {
      $final_version = $release['version'];
      $result = TRUE;
    }
    return $result ? $final_version : FALSE;
  }
  */
}

class DrushMakeDo_Whitelist extends DrushMakeDo_Transformer {
  // The list of currently allowed top-leval attributes.
  protected $attribute_whitelist = array();
  protected $level = '';

  function verify(&$makefile, $name = NULL) {
    $pass = TRUE;
    // Check for disallowed top-level attributes.
    foreach ($makefile as $attribute => $attribute_data) {
      if (!in_array($attribute, $this->attribute_whitelist)) {
        $this->log($this->error_message($attribute, $name));
        $pass = FALSE;
      }
    }
    return $pass;
  }

  function error_message($attribute, $name) {
    return dt("The attribute '%attribute' is not allowed.", array('%attribute' => $attribute));
  }
}

class DrushMakeDo_TopWhitelist extends DrushMakeDo_Whitelist {
  // The list of currently allowed top-level attributes.
  protected $attribute_whitelist = array('core', 'api', 'core_release', 'projects', 'libraries', 'includes', 'defaults');
  protected $level = 'top';
  function error_message($attribute, $name) {
    return dt("The top-level attribute '%attribute' is not allowed.", array('%attribute' => $attribute));
  }
}

class DrushMakeDo_ProjectWhitelist extends DrushMakeDo_Whitelist {
  protected $level = 'project';
  function error_message($attribute, $name) {
    return dt("The project-level attribute '%attribute' on project '%project' is not allowed.", array('%attribute' => $attribute, '%project' => $name));
  }
}

/**
 * Enforces valid project-level attributes for core-only makefiles.
 *
 * This child class only defines the attribute_whitelist protected member,
 * everything else is handled in the parent class.
 */
class DrushMakeDo_CoreProjectWhitelist extends DrushMakeDo_ProjectWhitelist {
  protected $attribute_whitelist = array('version', 'patch', 'download', 'type');
}

/**
 * Enforces valid project-level attributes for contrib-only makefiles.
 *
 * This child class only defines the attribute_whitelist protected member,
 * everything else is handled in the parent class.
 */
class DrushMakeDo_ContribProjectWhitelist extends DrushMakeDo_ProjectWhitelist {
  // The list of allowed project-level attributes for contrib makefiles.
  protected $attribute_whitelist = array('version', 'patch', 'download', 'type', 'subdir');
}

/**
 * Verify that download level attributes such as
 * `$project['foo']['download']['url']` are not allowed. (Meaning
 * only git from d.o. can be specified.)
 */
class DrushMakeDo_DownloadWhitelist extends DrushMakeDo_Whitelist {
  // The list of currently allowed project-download-level attributes.
  protected $attribute_whitelist = array('revision', 'branch', 'tag', 'type', 'url');

  /**
   * Need special handling for the type attribute (it can be set, but
   * must be 'git').
   */
  function verify(&$makefile, $name = NULL) {
    $pass = TRUE;
    if (isset($makefile['download'])) {
      $pass = parent::verify($makefile['download'], $name);
      if ($pass && isset($makefile['download']['type'])) {
        $pass = $makefile['download']['type'] === 'git';
      }
    }
    return $pass;
  }

  function error_message($attribute, $project) {
    return dt("The project download-level attribute '%attribute' on project '%project' is not allowed.", array('%attribute' => $attribute, '%project' => $project));
  }
}

/**
 * Verify that download level attribute 'revision' is used properly.
 *
 * The download level attribute 'branch' must be specified when 'revision' is
 * used (although if a version string is provided, we'll accept that as a
 * substitute). This is to ensure that we can properly re-write the .info
 * files such that update.module in core works.

 */
class DrushMakeDo_DownloadRevision extends DrushMakeDo_Transformer {

  function verify(&$makefile, $name = NULL) {
    if (isset($makefile['download']) && isset($makefile['download']['revision']) && empty($makefile['download']['branch']) && empty($makefile['version'])) {
      $this->log(dt("The project download-level attribute 'branch' must be defined when the 'revision' attribute is specified on project '%project'.", array('%project' => $name)));
      return FALSE;
    }
    return TRUE;
  }
}

/**
 * If it's a git download definition, set the project version.
 */
class DrushMakeDo_DownloadGit extends DrushMakeDo_Transformer {
  function verify(&$makefile, $name = NULL) {
    if (!empty($makefile['download']['type']) && $makefile['download']['type'] == 'git') {
      if (!empty($makefile['download']['tag'])) {
        $git_label = $makefile['download']['tag'];
        $is_branch = FALSE;
      }
      elseif (!empty($makefile['download']['branch'])) {
        $git_label = $makefile['download']['branch'];
        $is_branch = TRUE;
      }
      if (!empty($git_label)) {
        // Contrib projects embed core in the Git label, (e.g. '7.x-3.0')
        // which isn't what make is expecting (e.g. just the '3.0').
        if (drush_get_option('drupal-org') !== 'core') {
          $matches = array();
          if (preg_match('/^\d+\.x-(.+)$/', $git_label, $matches)) {
            $make_version = $matches[1];
          }
        }
        // However, core Git labels exactly match what make expects.
        else {
          $make_version = $git_label;
        }
        if (!empty($make_version)) {
          // If we're checking out from a branch, drush make expects the
          // version to include '-dev' on the end to more closely match the
          // drupal.org version strings.
          if ($is_branch) {
            $make_version .= '-dev';
          }
          $makefile['version'] = $make_version;
        }
      }
    }
    return TRUE;
  }
}

/**
 * Verify that download level attribute 'url' is used properly.
 *
 * At this point, we only allow 'url' if it's pointing to git.d.o.
 */
class DrushMakeDo_DownloadURL extends DrushMakeDo_Transformer {

  function verify(&$makefile, $name = NULL) {
    $pass = TRUE;
    if (isset($makefile['download']) && isset($makefile['download']['url'])) {
      if (!preg_match('@^(http|https|git)://git.drupal.org/.+$@', $makefile['download']['url'])) {
        $pass = FALSE;
        $this->log(dt("The download-level attribute 'url' can only be used if it is pointing to git.drupal.org. The URL for project '%project' is invalid (%url). ", array('%project' => $name, '%url' => $makefile['download']['url'])));
      }
    }
    return $pass;
  }
}

class DrushMakeDo_LibraryWhitelist extends DrushMakeDo_Transformer {
  protected $whitelist = NULL;

  function __construct() {
    $this->whitelist = $this->load_whitelist();
  }

  function whitelist_loaded() {
    return isset($this->whitelist) ? TRUE : FALSE;
  }

  function prepare_regex($regex) {
    $escaped_delimiter_regex = str_replace('@', '\@', $regex);
    return "@$escaped_delimiter_regex@";
  }

  function whitelist_url() {
    return drush_get_option('drupal-org-whitelist-url', DRUPALORG_DRUSH_PACKAGING_LIBRARY_WHITELIST);
  }

  function whitelist_url_data() {
    return $this->whitelist_url() . '/json';
  }

  function load_whitelist() {
    $whitelist_url = $this->whitelist_url_data();
    list($result, $data) = drupalorg_drush_get_remote_file($whitelist_url);
    $error_args = array('!whitelist_url' => $whitelist_url, '!error' => $data);
    switch ($result) {
      case 'success':
        $whitelist = json_decode($data);
        if (isset($whitelist)) {
          drush_log(dt("Library whitelist loaded from !whitelist_url:\n!whitelist", array('!whitelist_url' => $whitelist_url, '!whitelist' => var_export($whitelist, TRUE))), 'debug');
          return $whitelist;
        }
        else {
          $this->log(dt('Error parsing whitelist from !whitelist_url', $error_args));
          return NULL;
        }

      case 'failure':
        $this->log(dt("Whitelist download from !whitelist_url failed: !error", $error_args));
        return NULL;

      case 'not found':
        $this->log(dt("Whitelist download from !whitelist_url failed: not found", $error_args));
        return NULL;

    }
  }

  function verify($library_data, $name = NULL) {
    if ($url = $this->get_library_url($library_data)) {
      if ($this->whitelist_match($url, $name)) {
        return TRUE;
      }
      else {
        $this->log($this->error_message($url, $name));
      }
    }
    else {
      $this->log($this->error_message(dt('missing URL value'), $name));
    }
    return FALSE;
  }

  function get_library_url($library_data) {
    return !empty($library_data['download']['url']) ? $library_data['download']['url'] : NULL;
  }

  function whitelist_match($url, $name = NULL) {
    foreach ($this->whitelist as $regex) {
      drush_log(dt("Checking !url against regex '!regex'", array('!url' => $url, '!regex' => $regex)), 'debug');
      if (preg_match($this->prepare_regex($regex), $url)) {
        drush_log(dt("Library !name matches whitelist entry '!regex'", array('!name' => $name, '!regex' => $regex)), 'debug');
        return TRUE;
      }
    }
    return FALSE;
  }

  function error_message($url, $name) {
    return dt("The library '!library' cannot be downloaded from !url, the URL must be in the whitelist available at !whitelist.", array('!library' => $name, '!url' => $url, '!whitelist' => $this->whitelist_url()));
  }
}

class DrushMakeDo_ProjectPatch extends DrushMakeDo_Transformer {
  function verify(&$project_data, $project) {
    $pass = TRUE;
    // Check that patches do in fact come from drupal.org.
    if (isset($project_data['patch'])) {
      $allowed_patch_url = drush_get_option('drupal-org-allowed-patch-url', DRUPALORG_DRUSH_ALLOWED_PATCH_URL);
      foreach ($project_data['patch'] as $patch) {
        if (!preg_match($allowed_patch_url, $patch)) {
          make_error('BUILD_ERROR', dt("The patch '%patch' is not hosted on drupal.org", array('%patch' => $patch)));
          $pass = FALSE;
        }
      }
    }
    return $pass;
  }
}

/**
 * Verify that if the 'type' project attribute is set properly.
 *
 * If we're doing core-only validation, it must be 'core'. If we're doing
 * contrib-only validation, it must *not* be 'core'.
 */
class DrushMakeDo_ProjectType extends DrushMakeDo_Transformer {
  function verify(&$makefile, $name = NULL) {
    if (isset($makefile['type'])) {
      $drupal_org = drush_get_option('drupal-org');
      if ($drupal_org === 'core') {
        if ($makefile['type'] != 'core') {
          $this->log(dt("The project-level attribute 'type' must be set to 'core' for the %project project in a core-only make file.", array('%project' => $name)));
          return FALSE;
        }
      }
      else {
        if ($makefile['type'] == 'core') {
          $this->log(dt("The project-level attribute 'type' must not be set to 'core' for the %project project in a contrib-only make file.", array('%project' => $name)));
          return FALSE;
        }
      }
    }
    return TRUE;
  }
}

/**
 * Record the metadata for a given project.
 *
 * @param string $project
 *   The project machine name (key in the projects[] array in the .make file).
 * @param array $project_data
 *   Reference to an array of information about the project from the .make
 *   file. The 'type' attribute will be filled in based on release info if
 *   possible.
 * @param string $core
 *   The version of core specified in the .make file.
 *
 */
function drupalorg_drush_record_project_metadata($project, &$project_data, $core) {
  $metadata_file = drupalorg_drush_metadata_file();
  $package_nids = drush_get_option('drupal-org-log-package-items-to-file');
  // None of the rest of this matters unless at least one of the package
  // metadata command-line options has been set.
  if ($metadata_file || $package_nids) {
    $item_data = drupalorg_drush_get_project_metadata($project_data);
    $project_data += array(
      'name' => $project,
      'core' => $core,
      'location' => drush_get_option('make-update-default-url', RELEASE_INFO_DEFAULT_URL),
    );
    // In the case a release node exists, the only relevant data is the
    // release nid and any patches applied, drop everything else.
    if ($release = drupalorg_drush_find_release_node($project_data)) {
      $release_nid = basename($release['release_link']);
      $release_data = array(
        'nid' => $release_nid,
      );
      if (isset($item_data['patch'])) {
        $release_data['patch'] = $item_data['patch'];
      }
      $item_data = $release_data;
    }
    if ($metadata_file) {
      drupalorg_drush_metadata('project', $project, $item_data);
    }
    // TODO: rip this out once the JSON stuff is deployed.
    if ($package_nids) {
      drupalorg_drush_write_package_contents($release);
    }
  }
}

/**
 * Find the release node associated with a project, if any.
 *
 * @param array $project
 *   Reference to an array of information about a project. This must include
 *   version information so that the appropriate release can be located (if
 *   possible). The 'type' value in this array will be set to the project type
 *   as defined in the release history XML for the project.
 *
 * @return array
 *   An array of information about the release node from the release history
 *   XML data that represents the requested release of the given project. If
 *   no appropriate release node is found, this returns an empty array.
 */
function drupalorg_drush_find_release_node(&$project) {
  $type = $project['name'] == 'drupal' ? 'core' : 'contrib';
  $request = make_prepare_request($project, $type);
  $xml = updatexml_get_release_history_xml($request);
  if ($xml) {
    $request['project_type'] = $project['type'] = updatexml_determine_project_type($xml);
    // Try to identify the most appropriate release.
    if ($release = updatexml_parse_release($request, $xml, 'version')) {
      return $release;
    }
  }
  return array();
}

/**
 * Store a package item in a package summary file.
 *
 * TODO: Rip this out after JSON format is deployed.
 */
function drupalorg_drush_write_package_contents($release) {
  // Write the item's nid to the package contents file.
  $item_nid = basename($release['release_link']);
  if (!empty($item_nid)) {
    file_put_contents(drush_get_option('drupal-org-build-root') . '/' . DRUPALORG_DRUSH_PACKAGE_CONTENTS_FILE, "$item_nid\n", FILE_APPEND);
  }
}

/**
 * Write the saved package metadata to a file.
 *
 * @return boolean
 *   TRUE on success, FALSE if we can't write to the file (also calls
 *   make_error() to propagate the failure to the end-user).
 *
 * @see drupalorg_drush_metadata()
 * @see drupalorg_drush_metadata_file()
 */
function drupalorg_drush_write_package_metadata() {
  $json = json_encode(drupalorg_drush_metadata());
  $filepath = drupalorg_drush_metadata_file();
  if (!file_put_contents($filepath, $json, FILE_APPEND)) {
    make_error('BUILD_ERROR', dt("Failed to write '%file'", array('%file' => $filepath)));
    return FALSE;
  }
  return TRUE;
}

/**
 * Custom logging function for packaging errors.
 *
 * Logs all error messages to a build_errors.txt file in the root of the
 * package build.
 *
 * @see drush_log() for a description of the $entry array.
 */
function drupalorg_drush_log_errors_to_file($entry) {
  if ($entry['type'] == 'error' || $entry['type'] == 'failed') {
    file_put_contents(drush_get_option('drupal-org-build-root') . '/' . DRUPALORG_DRUSH_BUILD_ERRORS_FILE, $entry['message'] . "\n", FILE_APPEND);
  }
}

/**
 * Helper to get file contents from a URL using a variety of methods.
 *
 * Stolen from project_verify_package module.
 *
 * If PHP has libcurl loaded, use that. Otherwise if PHP is configured to allow
 * URLs in fopen(), use file_get_contents(). Finally, fall back to
 * attempting to execute wget from a pipe.
 *
 * @param $url
 *   URL for the file to fetch.
 * @return
 *   An array, first element is a return status string (one of 'success',
 *   'failure', or 'not found'), second element is the .make file on success,
 *   possibly an error message on failure, and nothing if the file is not
 *   found.
 */
function drupalorg_drush_get_remote_file($url) {
  $result = 'not found';
  $data = "";

  // Might be a local file.
  if (file_exists($url)) {
    $result = 'success';
    $data = file_get_contents($url);
  }
  // Try cURL if it exists.
  elseif (function_exists('curl_init')) {
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_TIMEOUT, 50);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
    curl_setopt($ch, CURLOPT_FOLLOWLOCATION, TRUE);
    // We need to set a user agent here, otherwise the request gets blocked from
    // drupal.org.
    curl_setopt($ch, CURLOPT_USERAGENT, 'Drush');
    $output = curl_exec($ch);
    // Most likely a server error.
    if ($output === FALSE) {
      $result = 'failure';
      $data = curl_error($ch);
    }
    // All HTTP codes other than 200 assume that the file does not exist on
    // the server.
    elseif (curl_getinfo($ch, CURLINFO_HTTP_CODE) == "200") {
      $result = 'success';
      $data = $output;
    }
    curl_close($ch);
  }
  // Use fopen if allowed.
  elseif (ini_get('allow_url_fopen')) {
    if ($output = file_get_contents($url)) {
      $result = 'success';
      $data = $output;
    }
  }
  // Last try, wget.
  else {
    $output = passthru('wget -q -O - ' . escapeshellarg($url), $return_value);
    if ($return_value === 0) {
      $result = 'success';
      $data = $output;
    }
  }

  return array($result, $data);
}

/**
 * Return the filename to use for logging package metadata (if any).
 *
 * @return string
 *   The filename to use for recording packaging metadata, or an empty string
 *   if this feature is not enabled.
 *
 * @see drupalorg_drush_write_package_metadata()
 */
function drupalorg_drush_metadata_file() {
  static $initialized = FALSE;
  static $metadata_file = '';
  if (!$initialized) {
    $initialized = TRUE;
    $log_metadata = drush_get_option('drupal-org-log-package-metadata');
    if (!empty($log_metadata)) {
      // If the option doesn't define a value, use the default filename.
      $metadata_file = drush_get_option('drupal-org-build-root') . '/';
      if ($log_metadata === TRUE) {
        $metadata_file .= DRUPALORG_DRUSH_PACKAGE_CONTENTS_JSON_FILE;
      }
      else {
        $metadata_file .= $log_metadata;
      }
    }
  }
  return $metadata_file;
}

/**
 * Helper function to harvest the useful packaging metadata from a project.
 *
 * @param array $project
 *   An array of project information from the .make file.
 *
 * @return array
 *   An array of relevant packaging metadata for this project. Includes the
 *   'version' attribute, 'branch', 'revision' and 'tag' from the 'download'
 *   definition, and the URLs of any patches applied to the project.
 *
 * @see drupalorg_drush_metadata()
 */
function drupalorg_drush_get_project_metadata($project) {
  $data = array();
  $top_attributes = array(
    'version',
  );
  foreach ($top_attributes as $attribute) {
    if (isset($project[$attribute])) {
      $data[$attribute] = $project[$attribute];
    }
  }
  if (isset($project['download'])) {
    $download_attributes = array(
      'branch',
      'revision',
      'tag',
    );
    foreach ($download_attributes as $attribute) {
      if (isset($project['download'][$attribute])) {
        $data[$attribute] = $project['download'][$attribute];
      }
    }
  }
  if (isset($project['patch'])) {
    $data['patch'] = array();
    foreach ($project['patch'] as $patch) {
      $data['patch'][] = $patch;
    }
  }
  return $data;
}

/**
 * Helper function to harvest the useful packaging metadata from a library.
 *
 * @param array $library
 *   An array of library information from the .make file.
 *
 * @return array
 *   An array of relevant packaging metadata for this library. Includes the
 *   URL the library is downloaded from, along with the URLs of any patches
 *   applied to the library.
 *
 * @see drupalorg_drush_metadata()
 */
function drupalorg_drush_get_library_metadata($library) {
  $data = array();
  if (isset($library['download'])) {
    $download_attributes = array(
      'url',
    );
    foreach ($download_attributes as $attribute) {
      if (isset($library['download'][$attribute])) {
        $data[$attribute] = $library['download'][$attribute];
      }
    }
  }
  if (isset($library['patch'])) {
    $data['patch'] = array();
    foreach ($library['patch'] as $patch) {
      $data['patch'][] = $patch;
    }
  }
  return $data;
}

/**
 * Store packaging metadata built by this invocation of drush make.
 *
 * This function is used to track any packaging metadata for things built
 * during the current invocation of drush make. If called with no arguments,
 * all of the packaging metadata is returned.
 *
 * @param string $type
 *   The type of packaging metadata to record. Can be either 'project' or
 *   'library'.
 * @param string $item
 *   The unique packaging key to identify the metadata. For example, a project
 *   machine name or the key in the libraries[] array in the .make file.
 * @param array $item_data
 *   An array of packaging metadata for the given item key.
 *
 * @return array
 *   Either an empty array (when data is being passed in) or the complete
 *   array of packaging metadata that has been saved if no arguments are
 *   defined.
 *
 * @see drupalorg_drush_metadata_file()
 * @see drupalorg_drush_get_project_metadata()
 * @see drupalorg_drush_get_library_metadata()
 * @see drupalorg_drush_write_package_metadata()
 */
function drupalorg_drush_metadata($type = '', $item = NULL, $item_data = array()) {
  static $metadata = array();
  if (!empty($type) && !empty($item)) {
    $metadata[$type][$item] = $item_data;
  }
  else {
    return $metadata;
  }
}
