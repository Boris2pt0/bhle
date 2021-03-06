<?php
// $Id$

/*
 *
 *
 * This Class implements the methods defined in the STANDARD_QT content model
 */

class Ffmpeg {
  function Ffmpeg() {
  }

  function extract_thumbnail($parameterArray, $dsid, $file, $file_ext) {
    $defaults = array('ss' => '00:00:10', 's' => null); 
    $params = array_merge($defaults, $parameterArray);
    $system = getenv('System');
    $file_suffix = '_'. $dsid . '.' . $file_ext;
    $returnValue=TRUE;
    $output=array();
    $size = '';

    if($params['s'] != null) {
      $size = ' -s ' . escapeshellarg($params['s']);
    }
    exec('ffmpeg -i ' . escapeshellarg($file) . ' -r 1 -ss ' . escapeshellarg($params['ss']) . ' ' . $size . ' -t 1 ' . escapeshellarg($file . $file_suffix));

    if (!file_exists($file . $file_suffix)) {
      return false;
    }

    $_SESSION['fedora_ingest_files']["$dsid"] = $file . $file_suffix;
    return TRUE;
  }
}
