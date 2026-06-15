<?php

module_load_include('inc', 'modularform', 'includes/api_auth');

class ModularformResponseResource extends RestWSEntityResourceController {

  public function read($id) {
    $response = db_select('modularform_responses', 'r')
      ->fields('r')
      ->condition('r.id', $id)
      ->execute()
      ->fetchObject();
    if (!$response) {
      throw new RestWSException('Response not found', 404);
    }

    $form = db_select('modularform_forms', 'f')
      ->fields('f', ['id', 'title'])
      ->condition('f.id', $response->form_id)
      ->execute()
      ->fetchObject();
    if (!$form) {
      throw new RestWSException('Form not found', 404);
    }

    $elements = _modularform_load_elements($response->form_id);
    $questions = array_filter($elements, fn($el) => $el['type'] === 'question');
    $data = _modularform_load_response_data($id);

    $answers = [];
    foreach ($data as $q_id => $entry) {
      $answers[$q_id] = [
        'label' => $questions[$q_id]['label'] ?? NULL,
        'type' => $entry['type'],
        'value' => $this->formatAnswer($entry),
      ];
    }

    return [
      'id' => (int) $id,
      'form_id' => (int) $response->form_id,
      'form_title' => $form->title,
      'user_id' => (int) $response->user_id,
      'email' => $response->email,
      'submitted_at' => (int) $response->submitted_at,
      'answers' => $answers,
    ];
  }

  public function wrapper($id) {
    $data = $this->read($id);
    // restws expects an EntityStructureWrapper but for custom data
    // we can use entity_metadata_wrapper on a stdClass
    return entity_metadata_wrapper('modularform_response', (object) $data);
  }

  protected function formatAnswer(array $entry) {
    $answer = $entry['answer'];
    $type = $entry['type'];
    $datatype_map = modularform_datatype_map();

    if (($datatype_map[$type] ?? NULL) === 'multi') {
      return array_map(fn($m) => [
        'id' => $m['id'],
        'label' => $m['label'],
      ], $entry['multi'] ?? []);
    }

    switch ($type) {
      case 'radio':
        return [
          'id' => $answer->value_int,
          'label' => $answer->option_label,
        ];

      case 'boolean':
        return (bool) $answer->value_bool;

      case 'date':
        return $answer->value_date ? date('Y-m-d', $answer->value_date) : NULL;

      case 'time':
        return $answer->value_date ? date('H:i', $answer->value_date) : NULL;

      case 'file':
        return array_map(fn($f) => [
          'fid' => $f['fid'],
          'filename' => $f['filename'],
          'url' => $f['url'],
          'filemime' => $f['filemime'],
        ], $entry['files'] ?? []);

      default: // text, textarea, richtext
        return $answer->value_varchar ?? $answer->value_text;
    }
  }

  public function create(array $values) {
    throw new RestWSException('Not supported', 405);
  }

  public function update($id, array $values) {
    throw new RestWSException('Not supported', 405);
  }

  public function delete($id) {
    throw new RestWSException('Not supported', 405);
  }

  public function access($op, $id) {
    $headers = getallheaders();
    $auth = isset($headers['Authorization']) ? $headers['Authorization'] : '';
    $ip = ip_address();

    if (!flood_is_allowed('modularform_api_ip_pre_auth', 100, 3600, $ip)) {
      drupal_add_http_header('Status', '429 Too Many Requests');
      drupal_json_output(['error' => 'Rate limit exceeded']);
      drupal_exit();
    }
    flood_register_event('modularform_api_ip_pre_auth', 3600, $ip);
    if (strpos($auth, 'Bearer ') === 0) {
      $token = substr($auth, 7);
      $decoded = modularform_jwt_verify($token);

      if (!$decoded) {
        return FALSE;
      }

      // Rate limit per user — 100 requests per hour
      $identifier = 'uid_' . $decoded->uid;
      if (!flood_is_allowed('modularform_api_access', 100, 3600, $identifier)) {
        drupal_add_http_header('Status', '429 Too Many Requests');
        drupal_json_output(['error' => 'Rate limit exceeded']);
        drupal_exit();
      }
      flood_register_event('modularform_api_access', 3600, $identifier);

      // Also rate limit per IP as a second layer
      if (!flood_is_allowed('modularform_api_access_ip', 200, 3600, $ip)) {
        drupal_add_http_header('Status', '429 Too Many Requests');
        drupal_json_output(['error' => 'Rate limit exceeded']);
        drupal_exit();
      }
      flood_register_event('modularform_api_access_ip', 3600, $ip);

      $this->_modularform_rate_limit_headers($decoded->uid);

      global $user;
      $original_user = $user;
      $user = user_load($decoded->uid);

      switch ($op) {
        case 'view':
          $access = response_view_access($id);
          break;
        case 'update':
          $access = response_edit_access($id);
          break;
        default:
          $access = FALSE;
      }

      $user = $original_user;
      return $access;
    }

    return FALSE;
  }

  protected function _modularform_rate_limit_headers($uid) {
    $identifier = 'uid_' . $uid;
    $limit = 100;
    $window = 3600;

    // Count current events in window
    $used = db_select('flood', 'f')
      ->condition('f.event', 'modularform_api_access')
      ->condition('f.identifier', $identifier)
      ->condition('f.timestamp', REQUEST_TIME - $window, '>')
      ->countQuery()
      ->execute()
      ->fetchField();

    drupal_add_http_header('X-RateLimit-Limit', $limit);
    drupal_add_http_header('X-RateLimit-Remaining', max(0, $limit - $used));
    drupal_add_http_header('X-RateLimit-Reset', REQUEST_TIME + $window);
  }

}
