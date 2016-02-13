<?php

/**
 * @file
 * Migration plugin to migrate movie data from IMDB to RMDB.
 */

class IMDBMovieMigration extends Migration {
  public function __construct($arguments = array()) {
    parent::__construct($arguments);
    $url = '';
    $source_type = variable_get('source_type', '');
    if ($source_type == 2) {
      $url = variable_get('migrate_url', '');
    }
    if ($source_type == 3) {
      $fid = variable_get('migrate_file', '');
      $file = file_load($fid);
      if ($file->fid) {
        $url = file_create_url($file->uri);
      }
    }

    $fields = array(
      'imdb_id' => 'IMDB unique id',
      'title' => 'Movie title',
      'plot' =>'Movie bio',
      'year' =>'Movie released year',
      'metascors' => 'Metascore',
      'imdb_rating' => 'Rating',
      'awards' => 'Awards',
      'runtime' => 'Duration',
      'imdb_votes' => 'Votes',
      'genre' => 'Genre',
      'country' => 'Country',
      'language' => 'Language',
      'director' => 'Director',
      'actors' => 'Actors',
      'poster' => 'Movie poster',
      'writer' => 'Writter',
      'released' => 'Release date',
    );

    // Source data.
    $this->source = new MigrateSourceJSON($url, 'imdb_id', $fields);

    // Destination data.
    $this->destination = new MigrateDestinationNode('movie');
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'imdb_id' => array(
          'type' => 'varchar',
          'not null' => TRUE,
          'length' => 10,
          'description' => 'IMDB Id',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // Direct field mappings.
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('body', 'plot');
    $this->addFieldMapping('field_year', 'year');
    $this->addFieldMapping('field_metascore', 'metascore');
    $this->addFieldMapping('field_rating', 'imdb_rating');
    $this->addFieldMapping('field_awards', 'awards');
    $this->addFieldMapping('field_run_time', 'runtime');
    $this->addFieldMapping('field_votes', 'imdb_votes');
    $this->addFieldMapping('field_released', 'released');

    // Term reference multivalued fields.
    $this->addFieldMapping('field_genre', 'genre');
    $this->addFieldMapping('field_country', 'country');
    $this->addFieldMapping('field_language', 'language');

    // Node reference fields processing.
    $this->addFieldMapping('field_director', 'director');
    $this->addFieldMapping('field_actors', 'actors');
    $this->addFieldMapping('field_writers', 'writer');

    // Image migrate
    $this->addFieldMapping('field_image', 'poster');
  }

  /**
   * {@inheritdoc}
   */
  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }
    if ($row->released == 'N/A') {
      $row->released = "";
    }
  }

  /**
   * {@inheritdoc}
   */
  public function prepare($node, stdClass $row) {
    $node->uid = 1;
    if (!empty($row->genre)) {
      $this->termProcessing($node, $row->genre, 'field_genre', 'genre');
    }
    if (!empty($row->country)) {
      $this->termProcessing($node, $row->country, 'field_country', 'country');
    }
    if (!empty($row->language)) {
      $this->termProcessing($node, $row->language, 'field_language', 'language');
    }
    if (!empty($row->director)) {
      $this->nodeProcessing($node, $row->director, 'field_director');
    }
    if (!empty($row->actors)) {
      $this->nodeProcessing($node, $row->actors, 'field_actors');
    }
    if (!empty($row->writer)) {
      $this->nodeProcessing($node, $row->writer, 'field_writers');
    }
  }

  /**
   * Processing the taxonomy terms reference field.
   *
   * @param type $node
   * @param type $terms
   * @param type $field_name
   * @param type $vocab_name
   */
  public function termProcessing($node, $terms, $field_name, $vocab_name) {
    $terms_array = explode(',', $terms);
    foreach ($terms_array as $key => $val) {
      $term = taxonomy_get_term_by_name($val, $vocab_name);
      if (!$term) {
        // Get the vocabulary id of the given vocab.
        $vid = db_select('taxonomy_vocabulary', 'tv')
            ->fields('tv', array('vid'))
            ->condition('tv.machine_name', $vocab_name)
            ->execute()->fetchField();
        $term = $this->createTerm($val, $vid);
        $node->{$field_name}[LANGUAGE_NONE][$key]['tid'] = $term->tid;
      }
      else {
        $node->{$field_name}[LANGUAGE_NONE][$key]['tid'] = key($term);
      }
    }
  }

  /**
   * Process the node reference fields.
   *
   * @param type $node
   * @param type $person
   * @param type $field_name
   */
  public function nodeProcessing($node, $person, $field_name) {
    $node_array = explode(',', $person);

    // We need extra processing for the writters.
    if ($field_name === 'field_writers') {
      $node_array = $this->writerProcessing($node_array);
    }
    foreach ($node_array as $key => $val) {
      // Check if node with given title already exist or not.
      $nid = db_select('node', 'n')
          ->fields('n', array('nid'))
          ->condition('n.title', $val)
          ->condition('n.type', 'actor_director_writter')
          ->execute()->fetchField();
      if (!$nid) {
        $new_node = $this->createNode($val, 'actor_director_writter');
        $node->{$field_name}[LANGUAGE_NONE][$key]['nid'] = $new_node->nid;
      }
      else {
        $node->{$field_name}[LANGUAGE_NONE][$key]['nid'] = $nid;
      }
    }
  }

  /**
   * Process the writters to filter unwanted things.
   *
   * @param type $writters
   * @return type array
   */
  public function writerProcessing($writters) {
    foreach ($writters as $key => $val) {
      $sub_explode = explode('(', $val);
      $writters[$key] = $sub_explode[0];
    }
    return $writters;
  }

  /**
   * Create the taxonomy term by name and vid.
   *
   * @param type $tname
   * @param type $vid
   * @return \stdClass
   */
  public function createTerm($tname, $vid) {
    $term = new stdClass();
    $term->name = $tname;
    $term->vid = $vid;
    taxonomy_term_save($term);
    return $term;
  }

  /**
   * Create the node by title and type.
   *
   * @param type $title
   * @param type $type
   * @return \stdClass
   */
  public function createNode($title, $type) {
    $node = new stdClass();
    $node->title = $title;
    $node->type = $type;
    $node->language = LANGUAGE_NONE;
    $node->uid = 1;
    node_save($node);
    return $node;
  }

  /**
   * Delete the referenced nodes on rollback.
   *
   * {@inheritdoc}
   */
  public function preRollback() {
    // Get migrated nodes.
    $query = db_select('migrate_map_migratemovie', 'mmm');
    $query->fields('mmm', array('destid1'));
    $result = $query->execute()->fetchAll();
    $dest = array();
    foreach ($result as $rs) {
      $dest[] = $rs->destid1;
    }

    $nodes = array();
    if (!empty($dest)) {
      // Get directors.
      $directors = db_select('field_data_field_director', 'dr')
        ->fields('dr', array('field_director_nid'))
        ->condition('dr.entity_id', $dest, 'IN')
        ->execute()->fetchAll();
      foreach ($directors as $dir) {
        $nodes[$dir->field_director_nid] = $dir->field_director_nid;
      }

      // Get actors.
      $actors = db_select('field_data_field_actors', 'ac')
        ->fields('ac', array('field_actors_nid'))
        ->condition('ac.entity_id', $dest, 'IN')
        ->execute()->fetchAll();
      foreach ($actors as $ac) {
        $nodes[$ac->field_actors_nid] = $ac->field_actors_nid;
      }

      // Get writers.
      $writers = db_select('field_data_field_writers', 'wr')
        ->fields('wr', array('field_writers_nid'))
        ->condition('wr.entity_id', $dest, 'IN')
        ->execute()->fetchAll();
      foreach ($writers as $wr) {
        $nodes[$wr->field_writers_nid] = $wr->field_writers_nid;
      }

      // Delete refernced nodes.
      node_delete_multiple($nodes);
    }
    parent::preRollback();
  }
}
