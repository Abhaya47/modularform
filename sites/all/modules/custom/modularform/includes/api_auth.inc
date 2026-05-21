<?php

function modularform_api_auth() {
  if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    drupal_add_http_header('Status', '405 Method Not Allowed');
    drupal_json_output(['error' => 'Method not allowed']);
    return;
  }

  $ip = ip_address();

  // Max 10 attempts per IP per hour
  if (!flood_is_allowed('modularform_auth', 10, 3600, $ip)) {
    drupal_add_http_header('Status', '429 Too Many Requests');
    drupal_json_output(['error' => 'Too many login attempts, try again later']);
    return;
  }

  $input = json_decode(file_get_contents('php://input'), TRUE);
  if (empty($input['username']) || empty($input['password'])) {
    drupal_add_http_header('Status', '400 Bad Request');
    drupal_json_output(['error' => 'Username and password required']);
    return;
  }

  $uid = user_authenticate($input['username'], $input['password']);
  if (!$uid) {
    // Register failed attempt
    flood_register_event('modularform_auth', 3600, $ip);
    drupal_add_http_header('Status', '401 Unauthorized');
    drupal_json_output(['error' => 'Invalid credentials']);
    return;
  }

  // Clear flood on success
  flood_clear_event('modularform_auth', $ip);

  $account = user_load($uid);
  $token   = modularform_jwt_issue($account);
  drupal_json_output(['token' => $token]);
}

function _modularform_jwt_secret() {
  // Store this in variables, not hardcoded
  return variable_get('modularform_jwt_secret', 'change-this-secret-in-production');
}

function modularform_jwt_issue($account) {
  require_once DRUPAL_ROOT . '/sites/all/libraries/php-jwt/src/JWT.php';
  require_once DRUPAL_ROOT . '/sites/all/libraries/php-jwt/src/Key.php';

  $payload = [
    'iss' => 'drupal',                    // issuer
    'iat' => REQUEST_TIME,                // issued at
    'exp' => REQUEST_TIME + (60 * 60 * 24), // expires in 24 hours
    'uid' => $account->uid,
    'name' => $account->name,
  ];

  return \Firebase\JWT\JWT::encode($payload, _modularform_jwt_secret(), 'HS256');
}

function modularform_jwt_verify($token) {
  require_once DRUPAL_ROOT . '/sites/all/libraries/php-jwt/src/JWT.php';
  require_once DRUPAL_ROOT . '/sites/all/libraries/php-jwt/src/Key.php';

  try {
    $decoded = \Firebase\JWT\JWT::decode(
      $token,
      new \Firebase\JWT\Key(_modularform_jwt_secret(), 'HS256')
    );
    return $decoded;
  }
  catch (\Firebase\JWT\ExpiredException $e) {
    return FALSE;
  }
  catch (\Exception $e) {
    return FALSE;
  }
}

