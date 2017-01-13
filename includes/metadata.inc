<?php

/**
 * @file
 * Various metadata related code, and the like.
 *
 * Sometimes we like to play with our metadata using php. Why not?
 *
 * @author Benjamin Rosner https://github.com/br2490
 *
 * The inspiration for this came from M. Jordan's islandora_metadata_extras.
 * He adds a mods:identifier'uuid' to all MODs records in his code.
 * Thank you @mjordan!
 *
 * @author: https://github.com/mjordan/
 * @see: https://github.com/mjordan/islandora_metadata_extras/tree/7.x
 */

/**
 * Master function for any specific DSID we want to update.
 * @todo: task functionality based on hooks.
 */
function bc_islandora_dsid_update($pid, $task = '', $commit) {
  module_load_include('inc', 'bc_islandora', 'includes/metadata');
  $job_status = array();
  
  // If we can get the object from fedora with pid return early.
  if (!$object = islandora_object_load($pid)) {
    drupal_set_message(t("PID doesn't exist at !pid.", array('!pid' => $pid)),
      'error');
    return;
  }
  // Put operations that should run always here.
  $job_status['Primary Display URL'] =
    _bc_islandora_generate_primary_dislpay_url($object, $commit);

  $job_status['HOCR Update'] = _bc_islandora_remove_HOCR_doctype($object, $commit);

  // If there are no tasks return early.
  if (empty($task)) {
    return $job_status;
  }

  return $job_status;
}

/**
 * Function that checks for and adds a primary display url to MODS.
 *
 * Barnard calls this function always to update mods on ingest with
 * the primary display URL.
 */
function _bc_islandora_generate_primary_dislpay_url($object, $commit) {
  global $base_url;
  global $base_path;
  $pid = $object->id;

  // Grab the datasteam we'll be working with. In this instance, MODS.
  $datastream = $object['MODS'];
  // Check to make sure there is actually a DATASTREAM there. If not return
  // early OR, in other scenarios, create the DS and populate.
  if (empty($datastream)) {
    drupal_set_message(t("There is no existing MODS record for !pid. Stopping.",
        array('!pid' => $pid)), 'warn');
    return 'NO_EXISTING_MODS_RECORDS';
  }

  $dom = new DOMDocument();
  $dom->preserveWhiteSpace = FALSE;
  $dom->formatOutput = TRUE;
  $dom->loadXML($datastream->content);
  $xpath = new DOMXPath($dom);
  $xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');

  $mods_locations = $dom->getElementsByTagName("location");
  if ($mods_locations->length === 0) {
    $root = $dom->firstChild;
    $location = $dom->createElementNS('http://www.loc.gov/mods/v3', 'location');
    $root->appendChild($location);
  }

  $existing_url_displays =
    $xpath->query("//mods:location/mods:url[@usage='primary display']");
  $existing_locations = $xpath->query('//mods:location');
  if ($existing_url_displays->length === 0) {
    foreach ($existing_locations as $loc) {
      $parent = $loc->parentNode->nodeName;

      // Here we modify "top level" mods:location(s).
      if ($parent == "mods") {
        // Create the primary dislpay url @ mods:location/mods:url.
        $link = $base_url . $base_path . 'islandora/object/' . $pid;
        $primary_display = $dom->createElementNS('http://www.loc.gov/mods/v3',
          'url', $link);
        $primary_display->setAttribute('usage', 'primary display');
        $loc->appendChild($primary_display);
        // Since we're adding a URL, we're getting rid of the the following:
        $loc->getAttribute('displayLabel') == "not a PURL" ?
          $loc->removeAttribute('displayLabel') : NULL;
      }
      // Here we modify other nodes that are not at the root level.
      /*
      elseif ($parent == "relatedItem") {
      dpm("trigger for related host items");
      Create the host item primary dislpay @ mods:host/mods:location/mods:url
      $link = $base_url . $base_path . 'islandora/object/' . urlencode($pid);
      $loc->hasAttribute('displayLabel') ?
      $loc->removeAttribute('displayLabel') : NULL;
      $primary_display = $dom->createElement('url', $link);
      $primary_display->setAttribute('usage', 'primary display');
      $loc->appendChild($primary_display);
      }
       */
    }

    $res = $dom->saveXML();
    if (empty($res)) {
      return 'COULD_NOT_CREATE_MODS';
    }

    if ($commit) {
      $datastream->setContentFromString($res);
      drupal_set_message(t("Updated metadata record at !pid.",
        array('!pid' => $pid)));
      return TRUE;
    }
    else {
      drupal_set_message(t("<strong>Preview</strong> of updated metadata record
        at !pid. Not pushed to Fedora.", array('!pid' => $pid)));
      dpm($res);
      return TRUE;
    }
  }
  else {
    drupal_set_message(t("Primary display URL already exists for !pid,
      skipping.", array('!pid' => $pid)), 'warn');
    return 'ALREADY_SET';
  }
}

/**
 * Function that checks for and adds a primary display url to MODS.
 *
 * Barnard calls this function always to update mods on ingest with
 * the primary display URL.
 */
function _bc_islandora_remove_HOCR_doctype($object, $commit) {
  $pid = $object->id;

  // Grab the datasteam we'll be working with. In this instance, MODS.
  $datastream = $object['HOCR'];
  // Check to make sure there is actually a DATASTREAM there. If not return
  // early OR, in other scenarios, create the DS and populate.
  if (empty($datastream)) {
    drupal_set_message(t("There is no existing HOCR record for !pid. Stopping.",
        array('!pid' => $pid)), 'warn');
    return 'NO_EXISTING_HOCR_RECORD';
  }

  // @see https://github.com/Islandora/islandora_ocr/blob/7.x/includes/hocr.inc#L571-L582
  $hocr_string = $datastream->content;
  // Wipe out the HTML DOCTYPE definition.
  $doctype_regex = '/<!DOCTYPE\s+html.*?>\s+/si';
  $hocr_res = preg_replace($doctype_regex, '', $hocr_string);

  if ($commit) {
    $datastream->setContentFromString($hocr_res);
    drupal_set_message(t("Updated HOCR record at !pid.",
      array('!pid' => $pid)));
    return TRUE;
  }
  else {
    drupal_set_message(t("<strong>Preview</strong> of updated HOCR record
      at !pid. Not pushed to Fedora.", array('!pid' => $pid)));
    dpm($hocr_res);
    return TRUE;
  }
}


/**
 * Metadata form.
 */
function bc_islandora_metadata_form() {
  $form['#attributes'] = array(
    'enctype' => 'multipart/form-data',
  );
  $form['pid'] = array(
    '#title' => t('Enter a single PID to regenerate or preview.'),
    '#type' => 'textfield',
    '#description' => t('PID to regenerate as namespace:identifier. <br />
      <strong>This field can accept URL encoded PIDs.</strong>'),
  );
  $form['csvfile'] = array(
    '#title' => t('CSV File'),
    '#type'  => 'file',
    '#description' => (t('<strong>Batch Mode: Engage.</strong><br />
      CSV file is simple: <strong>PID</strong>, <strong>OPERATION</strong>,
      [optional]<strong>COMMIT</strong>. <br />
      <strong>PID</strong>: are the objects to be updated;
      <strong>OPERATION</strong>: if defined in a hook, will call that hook.
      If left blank the default operations are performed;
      <strong>COMMIT</strong>: optional field, defaults to FALSE if not present.
      <br />
      Must always include headers in the CSV, checked for validity.')
    ),
  );

  $operations = module_invoke_all('barnard_islandora_metadata_functions');

  $operations_list = array();
  foreach ($operations as $machine_name => $profile) {
    $operations_list[$machine_name] = $profile['name'];
  }

  $form['task'] = array(
    '#title' => t('Task to run.'),
    '#type'  => 'radios',
    '#description' => (t('This is the hookable part.')),
    '#options' => $operations_list,
  );
  $form['commit'] = array(
    '#title' => t('Commit updated records to Fedora?'),
    '#type' => 'checkbox',
    '#required' => FALSE,
    '#default' => FALSE,
    '#description' => t('If checked updated records will be saved to Fedora.'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  $form['#validate'] = array(
    'bc_islandora_metadata_form_validate_fileupload',
    'bc_islandora_metadata_form_validate',
  );

  return $form;
}

/**
 * Validate the file upload.
 */
function bc_islandora_metadata_form_validate_fileupload(&$form, &$form_state) {
  $validators = array(
    'file_validate_extensions' => array('csv CSV'),
  );
  if ($file = file_save_upload('csvfile', $validators, 'temporary://')) {
    // The file was saved using file_save_upload() and was added to
    // the files table as a temporary file. We'll make a copy and let
    // the garbage collector delete the original upload.
    $csv_dir = 'temporary://bc_islandora';
    $directory_exists = file_prepare_directory($csv_dir, FILE_CREATE_DIRECTORY);
    if ($directory_exists) {
      $destination = $csv_dir . '/' . $file->filename;
      if (file_copy($file, $destination, FILE_EXISTS_REPLACE)) {
        $form_state['values']['csvupload'] = $destination;
      }
      else {
        form_set_error('bc_islandora_metadata', t('Unable to copy upload file
          to !dest', array('!dest' => $destination)));
      }
    }
  }
}

/**
 * Validate the upload.
 */
function bc_islandora_metadata_form_validate(&$form, &$form_state) {
  if (isset($form_state['values']['csvupload'])) {
    if ($handle = fopen($form_state['values']['csvupload'], 'r')) {
      $line_count = 1;
      $first = TRUE;
      if ($line = fgetcsv($handle, 4096)) {
        if (strtoupper($line[0]) != 'PID' ||
         strtoupper($line[1]) != 'OPERATION') {
          form_set_error('bc_islandora_metadata',
            t('Sorry, this file does not match the expected format.'));
        }
      }
      fclose($handle);
    }
    else {
      form_set_error('bc_islandora_metadata', t('Unable to read uploaded file
        !filepath', array('!filepath' => $form_state['values']['csvupload'])));
    }
  }
}

/**
 * Form submit handler.
 */
function bc_islandora_metadata_form_submit(&$form, &$form_state) {
  $pid = $form_state['values']['pid'];
  $commit = $form_state['values']['commit'];
  // $task = $form_state['values']['task'];
  
  // Determine our mode of operation and go.
  if ($pid) {
    drupal_set_message('Updating single record.');
    bc_islandora_dsid_update($pid, '', $commit);
  }

  elseif (isset($form_state['values']['csvupload'])) {
    $csv = $form_state['values']['csvupload'];

    $batch = array(
      'title' => t('Updating multiple records from CSV.'),
      'operations' => array(),
      'init_message' => t('Please wait...'),
      'progress_message' => t('Processed @current out of @total.'),
      'error_message' => t('An error occurred during processing.'),
      'finished' => 'bc_islandora_metadata_update_finished',
      'file' => drupal_get_path('module', 'bc_islandora') . '/includes/metadata.inc',
    );

    if ($handle = fopen($form_state['values']['csvupload'], 'r')) {
      $batch['operations'][] = array('_bc_islandora_metadata_remember_file',
        array($form_state['values']['csvupload']),
      );
      $line_count = 1;
      $first = TRUE;
      $line = fgetcsv($handle, 4096);
      while ($line = fgetcsv($handle, 4096)) {
        /*
         * we use base64_encode to ensure we don't overload the batch
         * processor by stuffing complex objects into it
         */
        $batch['operations'][] = array(
          '_bc_islandora_metadata_csv_row_proc',
          array(array_map('base64_encode', $line)),
        );
      }
      fclose($handle);
    }
    batch_set($batch);
  }
  else {
    form_set_error('bc_islandora_metadata', t('You gave me nothing to do!'));
  }
}

/**
 * Handle batch completion.
 */
function bc_islandora_metadata_update_finished($success, $results, $operations) {
  if (!empty($results['failed_rows'])) {
    $dir = 'public://bc_islandora';
    if (file_prepare_directory($dir, FILE_CREATE_DIRECTORY)) {
      $csv_filename = 'failed_rows-' . basename($results['uploaded_filename']);
      $csv_filepath = $dir . '/' . $csv_filename;
      $targs = array(
        '!csv_url' => l(check_plain($csv_filename),
          file_create_url($csv_filepath)),
        '%csv_filename' => $csv_filename,
        '%csv_filepath' => $csv_filepath,
      );
      if ($handle = fopen($csv_filepath, 'w+')) {
        foreach ($results['failed_rows'] as $failed_row) {
          fputcsv($handle, $failed_row);
        }
        fclose($handle);
        form_set_error('bc_islandora_metadata', t('Some rows failed to update.
          You may download a CSV of these rows: !csv_url', $targs)
        );
      }
      else {
        form_set_error('bc_islandora_metadata', t('Some rows failed to update,
          but unable to write error CSV to %csv_filepath', $targs)
        );
      }
    }
    else {
      form_set_error('bc_islandora_metadata', t('Some rows failed to update,
        but unable to create directory for error CSV at %csv_directory',
        $targs)
      );
    }
  }
  drupal_set_message(t('<strong>The CSV Metadata batch has completed.</strong>'));
}

/**
 * Remember the uploaded CSV filename.
 */
function _bc_islandora_metadata_remember_file($filename, &$context) {
  $context['results']['uploaded_filename'] = $filename;
}

/**
 * Process a single line from BATCH.
 */
function _bc_islandora_metadata_csv_row_proc($line, &$context) {
  $context['results']['rows_imported']++;
  $line = $cleaned_line = array_map('base64_decode', $line);

  $pid = $line[0];
  $task = $line[1];
  $commit = $line[2];
  $context['message'] = t('Updating PID !pid', array('!pid' => $pid));

  // @todo: Finish the hook part.
  /*
  $operations = module_invoke_all('barnard_islandora_metadata_functions');

  $operations_list = array();
  foreach ($operations as $machine_name => $profile) {
  $operations_list[$machine_name] = $profile['name'];
  }
   */

  $res = bc_islandora_dsid_update($pid, $task, $commit);

  if (empty($res)) {
    return 'OH JEEZ. ERROR UNDEFINED.';
  }

  foreach ($res as $job => $result) {
    if ($result !== TRUE) {
      $line[] = $job . ' returned: ' . $result;
      $context['results']['failed_rows'][] = $line;
    }
  }
}