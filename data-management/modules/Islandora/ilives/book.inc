<?php

// $Id$

class IslandoraBook {

  function  __construct($pid = '') {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    if (!empty($pid)) {

      $this->pid = $pid;
      $this->item = new FedoraItem($pid);
    }
  }

  public function buildDrupalForm($form = array(), $ingest_form = array(), &$form_state = array()) {
    $mods_save = '';
    if (isset($form_state['mods_save'])) {
      $mods_save = $form_state['mods_save'];
    }

    // Set #cache to true to create the $form_state cache
    $form['#cache'] = TRUE;
    
    // Give the user an option to enter a custom PID
    $form['custom_pid'] = array(
      '#type' => 'textfield',
      '#title' => 'Custom PID',
      '#description' => 'If you want to manually specify the PID for the new object, enter it here. '.
                        'Leave it blank for an automatically-generated PID.',
    );
    
    // Prompt the user to enter a record ID to be looked up in Evergreen.
    $form['unapi_url'] = array(
      '#type' => 'textfield',
      '#title' => 'Catalogue item URL',
    );
    $form['unapi_url_submit'] = array(
      '#type' => 'submit',
      '#value' => t('Retrieve MODS record'),
      '#submit' => array('fedora_ilives_retrieve_unapi_submit'),
      '#ahah' => array(
        'path' => 'fedora/ilives/retrieve_unapi/js', // path we defined in hook_menu
        'wrapper' => 'mods-wrapper', // the HTML that wraps the element that needs to be replaced
        'method' => 'replace', // the method we're going to use: a replace operation
        //'effect' => 'fade', // the effect used when replacing the element (try fade!)
      ),
      '#suffix' => '<br/>',
    );
    drupal_add_js('', 'inline');
    // We define a <div> wrapper. Everything in it will be replaced.
    $form['mods'] = array(
      '#tree' => TRUE,
      '#prefix' => '<div id="mods-wrapper">',
      '#suffix' => '</div>',
    );
    $form['mods']['mods_record'] = array(
      '#type' => 'textarea',
      '#title' => 'MODS Record to Import',
      '#rows' => 20,
      
    );
    
    if (!empty($mods_save)) {
      $form['mods']['mods_record']['#value'] = $mods_save['mods']['mods_record'];
    }
    return $form;
  }

  public function buildEditMetadataForm($form = array()) {

    $form['submit'] = array(
      '#type' => 'submit',
      '#weight' => 10,
      '#value' => 'Update'
    );
    $form['pid'] = array(
      '#type' => 'hidden',
      '#value' => $this->pid,
    );
    $form['dsid'] = array(
      '#type' => 'hidden',
      '#value' => "DARWIN_CORE",
    );

    return $this->buildDrupalForm($form);
  }

  public function handleEditMetadataForm($form_id, $form_values) {
    /*
     * Process the metadata form
     * Update the datastreams
     */

    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    module_load_include('inc', 'fedora_repository', 'plugins/DarwinCore');
    module_load_include('inc', 'fedora_repository', 'MimeClass');
    global $user;
    $mimetype = new MimeClass();
    $dwc = new DarwinCore($this->item);
    $dwc->handleForm($form_values);
    $this->item->purge_datastream('DARWIN_CORE');
    $this->item->add_datastream_from_string($dwc->darwinCoreXML, 'DARWIN_CORE',
            'Darwin Core Metadata', 'text/xml', 'X');
    return TRUE;
  }

  public function handleIngestForm($form_values, &$form_state) {
    /*
     * process the metadata form
     * Create fedora object
     * Add the datastreams
     */
    module_load_include('inc', 'fedora_repository', 'MimeClass');
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    if ($form_state['clicked_button']['#value'] == 'Retrieve MODS record') {
      return;
    }

    $mods_list_doc = new DomDocument();
    $mods_list_doc->loadXML($form_values['mods']['mods_record']);
    $mods_item_doc = new DomDocument();
    $mods_item = $mods_list_doc->getElementsByTagNameNS('http://www.loc.gov/mods/v3', 'mods')->item(0);
    $new_mods_item = $mods_item_doc->importNode($mods_item, TRUE);
    $mods_item_doc->appendChild($new_mods_item);
    
    $title_info = $mods_item_doc->getElementsByTagNameNS('http://www.loc.gov/mods/v3', 'titleInfo')->item(0);
    $title = '';
    foreach(array('nonSort', 'title') as $title_field) {
      $title .= $title_info->getElementsByTagNameNS('http://www.loc.gov/mods/v3', $title_field)->item(0)->nodeValue;
    }
    
    $mods_text = $mods_item_doc->saveXML();
    global $user;
    $mimetype = new MimeClass();
    
    $new_item = Fedora_Item::ingest_new_item(!empty($form_values['custom_pid']) ? $form_values['custom_pid'] : $form_values['pid'], 'A', $title,
            $user->name);
    
    $new_item->add_datastream_from_string($mods_text, 'MODS',
            'MODS Metadata', 'text/xml', 'X');

    $dc = transform_mods_to_dc($mods_text);
    if ($dc) {
      // Add the PID to a dc:identifier field.
      $dc_doc = simplexml_load_string($dc);
      $dc_doc->registerXPathNamespace('oai_dc', 'http://www.openarchives.org/OAI/2.0/oai_dc/');
      $dc_item = $dc_doc->xpath('//oai_dc:dc');
      foreach($dc_item as $node) {
        $node->addChild('dc:identifier', $new_item->pid, 'http://purl.org/dc/elements/1.1/');
      }
      $new_item->modify_datastream_by_value($dc_doc->saveXML(), 'DC', 'Dublin Core XML Metadata', 'text/xml');
    }
    $new_item->add_relationship('hasModel', $form_values['content_model_pid'], FEDORA_MODEL_URI);
    $new_item->add_relationship(!empty($form_values['relationship']) ? $form_values['relationship'] : 'isMemberOfCollection', $form_values['collection_pid']);
    drupal_set_message(t("Item !pid created successfully.", array('!pid' => l($new_item->pid, 'fedora/repository/'. $new_item->pid))), "status");

  }
  
  
  public function buildAddPagesForm($form = array()) {

  }

  public function showFieldSets() {
    global $user;
    global $base_url;
	drupal_add_library('system', 'ui.tabs');
	module_load_include('inc', 'fedora_repository', 'plugins/tagging_form');
    module_load_include('inc', 'fedora_repository', 'plugins/DarwinCore');
    module_load_include('module', 'fedora_ilives');	

    $tabs = '<script>jQuery(function() {jQuery( "#tabs" ).tabs();});</script>';
	$tabs .= '<div id="tabs"><ul>
				<li><a href="#tabs-1">'.t('Full-size').'</a></li>
				<li><a href="#tabs-2">'.t('Description').'</li>';
	if (user_access('administer nodes'))
		$tabs .= '<li><a href="#tabs-3">'.t('Add pages').'</a></li>';
	$tabs .= '</ul><div id="tabs-1"><p>';
    
    $qs = '';
    if ($user->uid != 0) {
      $qs = '?uid='. base64_encode($user->name . ':'. $user->pass);
    }

	$html = '<iframe src='.$base_url.'/'.drupal_get_path('module', 'fedora_repository').'/ilives/reader/index.html?pid=' . $this->pid.' scrolling="0" frameborder="0" style="width: 100%; height: 800px;">Errors: unable to load viewer</iframe>';

    $tabs.= $html . '</p></div><div id="tabs-2"><p>'.fedora_ilives_create_book_view($this->pid).'</p></div>';
	if (user_access('administer nodes'))
		$tabs .= '<div id="tabs-3"><p>'.drupal_render(drupal_get_form('book_add_pages_form', $this->pid)).'</p></div>';
	$tabs .= '</div>';
    return $tabs;
  }
}

function book_add_pages_form($form, &$form_state, $pid) {
  $form['pid'] = array(
    '#type' => 'hidden',
    '#value' => $pid,
  );

  $form['tiff_dir'] = array(
    '#type' => 'textfield',
    '#title' => t('TIFF folder'),
    '#required' => TRUE,
    '#description' => t('Path to a web-accessible folder contining the book\'s page images in TIFF format on this server.'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#weight' => 10,
    '#value' => 'Ingest Pages'
  );
  return $form;
}

function book_add_pages_form_validate($form, &$form_state) {
  if ($form_state['values']['tiff_dir'] == '') {
    form_set_error('', t('You must select a name for this group of settings.'));
  }
}

function book_add_pages_form_submit($form, &$form_state) {
  $pid = $form_state['values']['pid'];
  if (!empty($form_state['values']['tiff_dir'])) {
    $file_list = array();
    if ($tiff_dir = opendir($form_state['values']['tiff_dir'])) {
      while (FALSE !== ($file_name = readdir($tiff_dir))) {
        $ext = strrchr($file_name, '.');
        if ($ext == '.tif' || $ext == '.tiff') {
          array_push($file_list, $form_state['values']['tiff_dir'] .'/'. $file_name);
        }
      }
      closedir($tiff_dir);
      sort($file_list);
      ilives_create_page_objects($pid, $file_list);
    }
  }
}

/**
 * Create a page object for each element in $image_list as a child object of $pid
 */
function ilives_create_page_objects($pid, $image_list = array()) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $api = new FedoraAPI();
  $profile = $api->getObjectProfile($pid);
  if (!empty($profile->error)) {
    return FALSE;
  }
  $book = new FedoraItem($pid);
  $page_pids = array();
  $batch = array(
    'title' => 'Creating page objects',
    'operations' => array(),
    'file' => drupal_get_path('module', 'fedora_ilives') .'/book.inc',
  );
  $page_num = 1;
  $book_title = $book->get_label();
  if (strlen($book_title) > 100)
	$book_title = substr($book_title, 0, 35) . '...';
  foreach ($image_list as $image_path) {
    $batch['operations'][] = array('ilives_add_single_page_object', array($pid, $image_path, $page_num, "$book_title - Page ". sprintf("%03d", $page_num)));
    $page_num++;
  }
  batch_set($batch);
}

function ilives_add_single_page_object($book_pid, $page_file, $page_num = 1, $page_title = NULL) {var_dump($page_title);
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $api = new FedoraAPI();
  $pid = "$book_pid-". sprintf("%03d", $page_num);
  $api->ingest(NULL,NULL,$pid,TRUE, $page_title);
  //$page_item = FedoraItem::ingest_new_item("$book_pid-". sprintf("%03d", $page_num), 'A', $page_title);
  $page_item = new FedoraItem($pid);
  $page_item->add_relationship('hasModel', 'ilives:pageCModel', FEDORA_MODEL_URI);
  $page_item->add_relationship('isMemberOf', $book_pid);
  $page_item->addDataStream('TIFF', $page_file, null,
	array('dsLabel'=>'Archival TIFF', 'mimeType'=>'image/tiff', 'controlGroup'=>'M'));
}

function transform_mods_to_dc($mods) {
  $xp = new XsltProcessor();
  // create a DOM document and load the XSL stylesheet
  $xsl = new DomDocument;
  $xsl->load(drupal_get_path('module', 'fedora_ilives').'/xsl/MODS3-22simpleDC.xsl');
  
  // import the XSL styelsheet into the XSLT process
  $xp->importStylesheet($xsl);
  
   // create a DOM document and load the XML datat
  $xml_doc = new DomDocument;
  $xml_doc->loadXML($mods);
  
   // transform the XML into HTML using the XSL file
  if ($dc = $xp->transformToXML($xml_doc)) {
    return $dc;
  }
  else {
    return FALSE;
  }
}
