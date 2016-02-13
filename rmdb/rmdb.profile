<?php

/**
 * @file
 * Profile file works similar like module during insallation.
 */

/**
 * Implements hook_form_FORM_ID_alter().
 */
function rmdb_form_install_settings_form_alter(&$form, $form_state) {
  $form['mysql']['db_prefix']['#access'] = FALSE;
}
