<?php

require_once __DIR__ . '/../vendor/autoload.php';

class AcsfMessageUnitTestMissingEndpoint extends \Drupal\acsf\AcsfMessage {

  /**
   * Implements AcsfMessage::sendMessage().
   */
  protected function sendMessage($url, $method, $endpoint, array $parameters, $username, $password) {
    return new AcsfMessageResponseUnitTest(NULL, 0, 'response body');
  }

}

