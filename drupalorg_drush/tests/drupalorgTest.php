<?php

/**
 * @file
 *   PHPUnit Tests for Drupal.org drush commands.
 *
 *   To run the tests, use phpunit --bootstrap=/path/to/drush/tests/drush_testcase.inc.
 *   Note that we are pointing to the drush_testcase.inc file under /tests subdir in drush.
 */

/**
 * Drush make-specific tests.
 */
class drupalorgDrushMakeTestCase extends Drush_CommandTestCase {
  /**
   * Run a given makefile test. Similar to tests in makeMakefileCase.
   *
   * @param $test
   *   The test makefile to run, as defined by $this->getMakefile();
   */
  private function runMakefileTest($test) {
    $default_options = array(
      // Add the --include option so the tests can find this code.
      'include' => dirname(__FILE__) . '/..',
      'test' => NULL,
      'md5' => 'print',
      'drupal-org' => NULL,
      'drupal-org-whitelist-url' => 'file://' . dirname(__FILE__) . "/packaging_whitelist_data",
    );
    $makefile_path = dirname(__FILE__) . '/makefiles';
    $config = $this->getMakefile($test);
    $options = $config['options'] + $default_options;
    $makefile = $makefile_path . '/' . $config['makefile'];
    $return = !empty($config['fail']) ? self::EXIT_ERROR : self::EXIT_SUCCESS;
    $command = isset($config['command']) ? $config['command'] : 'make';
    $this->drush($command, array($makefile), $options, NULL, NULL, $return);

    if (empty($config['fail']) && !empty($config['md5'])) {
      // Check the log for the build hash.
      $output = $this->getOutputAsList();
      $this->assertEquals($output[0], $config['md5'], $config['name'] . ' - build md5 matches expected value: ' . $config['md5']);
    }
  }

  function testMakeDoFailAttribute() {
    $this->runMakefileTest('do-make-fail-attribute');
  }

  function testMakeDoPassDev() {
    $this->runMakefileTest('do-make-pass-dev');
  }

  function testMakeDoFailLibrary() {
    $this->runMakefileTest('do-make-fail-library');
  }

  function testMakeDoFailCoreAndContribAsCore() {
    $this->runMakefileTest('do-make-fail-core-and-contrib-as-core');
  }

  function testMakeDoFailCoreAndContribAsContrib() {
    $this->runMakefileTest('do-make-fail-core-and-contrib-as-contrib');
  }

  function testMakeDoFailCoreAndLibrary() {
    $this->runMakefileTest('do-make-fail-core-and-library');
  }

  function testMakeSucceedLibrary() {
    $this->runMakefileTest('do-make-succeed-library');
  }

  function testMakeDoFailPatch() {
    $this->runMakefileTest('do-make-fail-patch');
  }

  function testMakeDoSucceed() {
    $this->runMakefileTest('do-make-succeed');
  }

  function testMakeDoSucceedHttps() {
    $this->runMakefileTest('do-make-succeed-https');
  }

  function testMakeDoFailBranch() {
    $this->runMakefileTest('do-make-fail-branch');
  }

  function testMakeDoFailNoBranch() {
    $this->runMakefileTest('do-make-fail-no-branch');
  }

  function testMakeDoSucceedRevisionWithBranch(){
    $this->runMakefileTest('do-make-succeed-revision-with-branch');
  }

  function testMakeDoSucceedRevisionWithVersion(){
    $this->runMakefileTest('do-make-succeed-revision-with-version');
  }

  function testVerifyMakefileDoFailAttribute() {
    $this->runMakefileTest('do-verify-makefile-fail-attribute');
  }

  function testVerifyMakefileDoPassDev() {
    $this->runMakefileTest('do-verify-makefile-pass-dev');
  }

  function testVerifyMakefileDoFailLibrary() {
    $this->runMakefileTest('do-verify-makefile-fail-library');
  }

  function testVerifyMakefileDoFailCoreAndContribAsCore() {
    $this->runMakefileTest('do-verify-makefile-fail-core-and-contrib-as-core');
  }

  function testVerifyMakefileDoFailCoreAndContribAsContrib() {
    $this->runMakefileTest('do-verify-makefile-fail-core-and-contrib-as-contrib');
  }

  function testVerifyMakefileDoFailCoreAndLibrary() {
    $this->runMakefileTest('do-verify-makefile-fail-core-and-library');
  }

  function testVerifyMakefileSucceedLibrary() {
    $this->runMakefileTest('do-verify-makefile-succeed-library');
  }

  function testVerifyMakefileDoFailPatch() {
    $this->runMakefileTest('do-verify-makefile-fail-patch');
  }

  function testVerifyMakefileDoSucceed() {
    $this->runMakefileTest('do-verify-makefile-succeed');
  }

  function testVerifyMakefileDoSucceedHttps() {
    $this->runMakefileTest('do-verify-makefile-succeed-https');
  }

  function testVerifyMakefileDoFailBranch() {
    $this->runMakefileTest('do-verify-makefile-fail-branch');
  }

  function testVerifyMakefileDoFailNoBranch() {
    $this->runMakefileTest('do-verify-makefile-fail-no-branch');
  }

  function testVerifyMakefileDoSucceedRevisionWithBranch(){
    $this->runMakefileTest('do-verify-makefile-succeed-revision-with-branch');
  }

  function testVerifyMakefileDoSucceedRevisionWithVersion(){
    $this->runMakefileTest('do-verify-makefile-succeed-revision-with-version');
  }

  function testVerifyMakefileDoSucceedCoreOnly(){
    $this->runMakefileTest('do-verify-makefile-succeed-core-only');
  }

  function testVerifyMakefileDoFailInclude(){
    $this->runMakefileTest('do-verify-makefile-fail-include');
  }

  function testVerifyMakefileDoSucceedInclude(){
    $this->runMakefileTest('do-verify-makefile-succeed-include');
  }

  function getMakefile($key) {
    static $tests = array(
      'do-make-fail-attribute' => array(
        'name'     => 'D.o: Fail attributes',
        'makefile' => 'do-fail-attribute.make',
        'fail'    => TRUE,
        'options' => array(),
      ),
      'do-make-pass-dev' => array(
        'name'     => 'D.o: pass dev',
        'makefile' => 'do-pass-dev.make',
        'md5' => 'a938f76ef404146fac6b05775d1377d1',
        'options' => array(),
      ),
      'do-make-fail-library' => array(
        'name'     => 'D.o: Fail library',
        'makefile' => 'do-fail-library.make',
        'fail'    => TRUE,
        'options' => array(),
      ),
      'do-make-succeed-library' => array(
        'name'     => 'D.o: Success library',
        'makefile' => 'do-succeed-library.make',
        'md5'    => 'e1f749d6aad0e2509fefc715f72a3ceb',
        'options' => array(),
      ),
      'do-make-fail-core-and-contrib-as-contrib' => array(
        'name'     => 'D.o: Fail core and contrib using contrib validation',
        'makefile' => 'do-fail-core-and-contrib.make',
        'fail'    => TRUE,
        'options' => array('drupal-org' => 'contrib'),
      ),
      'do-make-fail-core-and-contrib-as-core' => array(
        'name'     => 'D.o: Fail core and contrib using core validation',
        'makefile' => 'do-fail-core-and-contrib.make',
        'fail'    => TRUE,
        'options' => array('drupal-org' => 'core'),
      ),
      'do-make-fail-core-and-library' => array(
        'name'     => 'D.o: Fail core and library',
        'makefile' => 'do-fail-core-and-library.make',
        'fail'    => TRUE,
        'options' => array('drupal-org' => 'core'),
      ),
      'do-make-fail-patch' => array(
        'name'     => 'D.o: Fail patch',
        'makefile' => 'do-fail-patch.make',
        'fail'    => TRUE,
        'options' => array(),
      ),
      'do-make-succeed' => array(
        'name'     => 'D.o: Success',
        'makefile' => 'do-succeed.make',
        'md5' => '776b929663eafddeb58240202dc7308a',
        'options' => array(),
      ),
      'do-make-succeed-https' => array(
        'name'     => 'D.o: Success (patch over https)',
        'makefile' => 'do-succeed-https.make',
        // This should be the same md5 hash as above, except that the
        // generated PATCHES.txt file has the full patch URL, which is
        // different, and therefore the resulting hash is different.
        'md5' => '7f8fc2a6f1ac290f40794e52fb158126',
        'options' => array(),
      ),
      'do-make-fail-branch' => array(
        'name' => "D.o: Branch with 'url' specified",
        'makefile' => 'do-fail-branch.make',
        'fail'    => TRUE,
        'options' => array(),
      ),
      'do-make-fail-no-branch' => array(
        'name' => 'D.o: Branch with no revision',
        'makefile' => 'do-fail-no-branch.make',
        'fail' => TRUE,
        'options' => array(),
      ),
      'do-make-succeed-revision-with-branch' => array(
        'name' => 'D.o: Branch with revision and branch specified',
        'makefile' => 'do-succeed-revision-with-branch.make',
        'md5' => '51f2678491f7bc76534fd08d1860fdf9',
        'options' => array('no-gitinfofile' => NULL),
      ),
      'do-make-succeed-revision-with-version' => array(
        'name' => 'D.o: Branch with revision and version specified',
        'makefile' => 'do-succeed-revision-with-version.make',
        'md5' => '51f2678491f7bc76534fd08d1860fdf9',
        'options' => array('no-gitinfofile' => NULL),
      ),
      'do-verify-makefile-fail-attribute' => array(
        'name'     => 'D.o: Fail attributes',
        'makefile' => 'do-fail-attribute.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-pass-dev' => array(
        'name'     => 'D.o: pass dev',
        'makefile' => 'do-pass-dev.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-fail-library' => array(
        'name'     => 'D.o: Fail library',
        'makefile' => 'do-fail-library.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed-library' => array(
        'name'     => 'D.o: Success library',
        'makefile' => 'do-succeed-library.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-fail-core-and-contrib-as-contrib' => array(
        'name'     => 'D.o: Fail core and contrib using contrib validation',
        'makefile' => 'do-fail-core-and-contrib.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array('drupal-org' => 'contrib'),
      ),
      'do-verify-makefile-fail-core-and-contrib-as-core' => array(
        'name'     => 'D.o: Fail core and contrib using core validation',
        'makefile' => 'do-fail-core-and-contrib.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array('drupal-org' => 'core'),
      ),
      'do-verify-makefile-fail-core-and-library' => array(
        'name'     => 'D.o: Fail core and library',
        'makefile' => 'do-fail-core-and-library.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array('drupal-org' => 'core'),
      ),
      'do-verify-makefile-fail-patch' => array(
        'name'     => 'D.o: Fail patch',
        'makefile' => 'do-fail-patch.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed' => array(
        'name'     => 'D.o: Success',
        'makefile' => 'do-succeed.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed-https' => array(
        'name'     => 'D.o: Success (patch over https)',
        'makefile' => 'do-succeed-https.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-fail-branch' => array(
        'name' => "D.o: Branch with 'url' specified",
        'makefile' => 'do-fail-branch.make',
        'fail'    => TRUE,
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-fail-no-branch' => array(
        'name' => 'D.o: Branch with no revision',
        'makefile' => 'do-fail-no-branch.make',
        'fail' => TRUE,
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed-revision-with-branch' => array(
        'name' => 'D.o: Branch with revision and branch specified',
        'makefile' => 'do-succeed-revision-with-branch.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed-revision-with-version' => array(
        'name' => 'D.o: Branch with revision and version specified',
        'makefile' => 'do-succeed-revision-with-version.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed-core-only' => array(
        'name' => 'D.o: Verify with only core project',
        'makefile' => 'do-pass-verify-core-only.make',
        'command' => 'verify-makefile',
        'options' => array('drupal-org' => 'core'),
      ),
      'do-verify-makefile-fail-include' => array(
        'name' => 'D.o: Invalid include file used',
        'makefile' => 'do-fail-include.make',
        'fail' => TRUE,
        'command' => 'verify-makefile',
        'options' => array(),
      ),
      'do-verify-makefile-succeed-include' => array(
        'name' => 'D.o: Valid include file used',
        'makefile' => 'do-succeed-include.make',
        'command' => 'verify-makefile',
        'options' => array(),
      ),

    );
    return $tests[$key];
  }
}
