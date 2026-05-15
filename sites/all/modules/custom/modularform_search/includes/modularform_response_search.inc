<?php

#TODO pagination

module_load_include('inc', 'modularform', 'modularform.rules');

function modularform_search_get_questions($form_label) {
  $solr = apachesolr_get_solr();
  $url  = $solr->getUrl() . 'select';

  $choice_types = [2, 4, 6, 7, 11]; // radio, checkbox, dropdown, tag, list
  $skip_types   = [5, 9, 10];        // file, date, time

  $params = http_build_query([
    'q'    => '{!child of="ss_doc_type:response AND ss_form_label:\"' . addslashes($form_label) . '\""}ss_doc_type:answer',    'rows' => 1000,
    'fl'   => 'ss_q_id,ss_q_label,is_q_type,ss_val,sm_val',
    'wt'   => 'json',
  ]);

  $response = drupal_http_request($url . '?' . $params);

  if ($response->code != 200) {
    watchdog('modularform_search', 'Solr question fetch failed: @error',
      ['@error' => $response->data], WATCHDOG_ERROR);
    return [];
  }

  $data = json_decode($response->data, TRUE);
  $docs = isset($data['response']['docs']) ? $data['response']['docs'] : [];

  $questions = [];

  foreach ($docs as $doc) {
    if (empty($doc['ss_q_id'])) {
      continue;
    }

    $q_id   = $doc['ss_q_id'];
    $q_type = isset($doc['is_q_type']) ? (int) $doc['is_q_type'] : 0;

    if (in_array($q_type, $skip_types)) {
      continue;
    }

    if (!isset($questions[$q_id])) {
      $questions[$q_id] = [
        'q_id'    => $q_id,
        'label'   => isset($doc['ss_q_label']) ? trim($doc['ss_q_label']) : $q_id,
        'type'    => $q_type,
        'options' => [],
      ];
    }

    if (in_array($q_type, $choice_types)) {
      if (!empty($doc['sm_val']) && is_array($doc['sm_val'])) {
        foreach ($doc['sm_val'] as $val) {
          $questions[$q_id]['options'][$val] = $val;
        }
      }
      elseif (!empty($doc['ss_val'])) {
        $val = $doc['ss_val'];
        $questions[$q_id]['options'][$val] = $val;
      }
    }
  }

  foreach ($questions as &$q) {
    $q['options'] = array_values($q['options']);
  }

  return array_values($questions);
}

/**
 * Build and execute a filtered response search.
 *
 * @param string $form_label
 *   The form label to search within.
 * @param array $filters
 *   Array of filters, each with:
 *   - label: the question label (ss_q_label)
 *   - value: the answer value to match
 *   - type: the question type int (drives which field to search)
 * @param int $offset
 * @param int $limit
 *
 * @return array
 */
function modularform_search_get_responses($form_label, array $filters = [], $offset = 0, $limit = 20) {
  $solr = apachesolr_get_solr();
  $url  = $solr->getUrl() . 'select';

  // Base query — all responses for this form
  $fq = 'ss_doc_type:response AND ss_form_label:"' . addslashes($form_label) . '"';

  // Each filter becomes one {!parent} block ANDed together
  $parent_filters = [];
  foreach ($filters as $filter) {
    $q_label  = addslashes($filter['label']);
    $q_value  = addslashes($filter['value']);
    $q_type   = (int) $filter['type'];
    $solr_val_field = modularform_search_val_field_for_type($q_type);

    $parent_filters[] = '{!parent which="ss_doc_type:response"}'
      . '(ss_q_label:"' . $q_label . '" AND ' . $solr_val_field . ':"' . $q_value . '")';
  }

  // Build final q
  if (!empty($parent_filters)) {
    $q = $fq . ' AND ' . implode(' AND ', $parent_filters);
  }
  else {
    $q = $fq;
  }

  $params = [
    'q'    => $fq,
    'rows' => $limit,
    'start'=> $offset,
    'fl'   => '*,[child]',
    'wt'   => 'json',
  ];

  foreach ($parent_filters as $i => $pf) {
    $params['fq[' . $i . ']'] = $pf;
  }

  $response = drupal_http_request($url . '?' . $params);

  if ($response->code != 200) {
    watchdog('modularform_search', 'Solr response search failed: @error',
      ['@error' => $response->data], WATCHDOG_ERROR);
    return ['total' => 0, 'responses' => []];
  }

  $data = json_decode($response->data, TRUE);
  return modularform_search_parse_responses($data);
}

/**
 * Resolve which Solr value field to search for a given question type int.
 * Multi-value types search sm_val, others use the solr_field_map via storage type.
 */
function modularform_search_val_field_for_type($type_int) {
  $multi_types = [4, 7, 11]; // checkbox, tag, list
  if (in_array($type_int, $multi_types)) {
    return 'sm_val';
  }

  $type_map    = array_flip(modularform_type_map());
  $datatype_map   = modularform_datatype_map();
  $solr_field_map = solr_field_map();

  $type_name = isset($type_map[$type_int]) ? $type_map[$type_int] : NULL;
  $storage   = isset($datatype_map[$type_name]) ? $datatype_map[$type_name] : 'varchar';

  return isset($solr_field_map[$storage]) ? $solr_field_map[$storage] : 'ss_val';
}

/**
 * Parse raw Solr response into clean response + answer shape.
 */
function modularform_search_parse_responses(array $data) {
  $total = isset($data['response']['numFound']) ? (int) $data['response']['numFound'] : 0;
  $docs  = isset($data['response']['docs'])     ? $data['response']['docs']           : [];

  $responses = [];

  foreach ($docs as $doc) {
    $children = isset($doc['_childDocuments_']) ? $doc['_childDocuments_'] : [];

    $answers = [];
    foreach ($children as $child) {
      if (empty($child['ss_q_id'])) {
        continue;
      }

      // Pick whichever value field is present
      $value = NULL;
      if (isset($child['sm_val'])) {
        $value = $child['sm_val']; // array
      }
      elseif (isset($child['ss_val'])) {
        $value = $child['ss_val'];
      }
      elseif (isset($child['ts_val'])) {
        $value = $child['ts_val'];
      }
      elseif (isset($child['is_val'])) {
        $value = $child['is_val'];
      }
      elseif (isset($child['bs_val'])) {
        $value = $child['bs_val'];
      }
      elseif (isset($child['ds_val'])) {
        $value = $child['ds_val'];
      }

      $answers[] = [
        'q_id'   => $child['ss_q_id'],
        'label'  => isset($child['ss_q_label']) ? trim($child['ss_q_label']) : $child['ss_q_id'],
        'type'   => isset($child['is_q_type']) ? (int) $child['is_q_type'] : NULL,
        'value'  => $value,
      ];
    }

    $responses[] = [
      'response_id'  => $doc['id'],
      'email'        => isset($doc['ss_email'])        ? $doc['ss_email']        : NULL,
      'user_id'      => isset($doc['is_user_id'])      ? (int) $doc['is_user_id'] : NULL,
      'submitted_at' => isset($doc['is_submitted_at']) ? (int) $doc['is_submitted_at'] : NULL,
      'answers'      => $answers,
    ];
  }

  return [
    'total'     => $total,
    'responses' => $responses,
  ];
}
