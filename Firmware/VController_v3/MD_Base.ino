// Please read VController_v3.ino for information about the license and authors

// This page has the following parts:
// Section 1: Device Initialization
// Section 2: Device common MIDI in functions
// Section 3: Device common MIDI out functions
// Section 4: Device program change
// Section 5: Effect parameter and assign control
// Section 6: Looper control

// The MD_base_class is the base class for all the devices

// ********************************* Section 1: Device Initialization ********************************************
// Types for sysex requests:
#define REQUEST_PATCH_NAME 1
#define REQUEST_PARAMETER_TYPE 2
#define REQUEST_ASSIGN_TYPE 3
#define REQUEST_FX_BUTTON_TYPE 4

// Constructor - called by all derived classes as well
MD_base_class::MD_base_class (uint8_t _dev_no) {
  my_device_number = _dev_no;
}

// Initialize device variables
// Called at startup of VController
void MD_base_class::init() { // Default values for variables
  enabled = DEVICE_OFF;
  my_device_page1 = PAGE_CURRENT_PATCH_BANK;
  my_device_page2 = 0;
  my_device_page3 = 0;
  my_device_page4 = 0;

  current_patch_name.reserve(17);
  current_patch_name = "                ";
  is_always_on = true; // Default value
}

void MD_base_class::update() {}

// Functions for EEPROM/menu to get and set values
// Make sure you update NUMBER_OF_DEVICE_SETTINGS on page Classes when you add a new setting

uint8_t MD_base_class::get_setting(uint8_t variable) {
  switch (variable) {
    case 0: return my_LED_colour;
    case 1: return MIDI_channel;
    case 2: return MIDI_port_manual;
    case 3: return MIDI_device_id;
    case 4: return bank_number;
    case 5: return is_always_on;
    case 6: return my_device_page1;
    case 7: return my_device_page2;
    case 8: return my_device_page3;
    case 9: return my_device_page4;
    case 10: return enabled;
    case 11: return external_MIDI_port;
    case 12: return external_MIDI_channel;
  }
  return 0;
}

void MD_base_class::set_setting(uint8_t variable, uint8_t value) {
  switch (variable) {
    case 0: my_LED_colour = value; break;
    case 1: MIDI_channel = value; break;
    case 2: MIDI_port_manual = value; break;
    case 3: MIDI_device_id = value; break;
    //case 4: bank_number = value; break;
    case 5: is_always_on = value; break;
    case 6: my_device_page1 = value; break;
    case 7: my_device_page2 = value; break;
    case 8: my_device_page3 = value; break;
    case 9: my_device_page4 = value; break;
    case 10: enabled = value; break;
    case 11: external_MIDI_port = value; break;
    case 12: external_MIDI_channel = value; break;
  }
}

// ********************************* Section 2: Device common MIDI in functions ********************************************

void MD_base_class::check_SYSEX_in(const unsigned char* sxdata, short unsigned int sxlength, uint8_t port) {}

void MD_base_class::forward_MIDI_message(const unsigned char* sxdata, short unsigned int sxlength) {}

void MD_base_class::check_PC_in(uint8_t program, uint8_t channel, uint8_t port) {
  // Check the source by checking the channel
  if ((port == MIDI_port) && (channel == MIDI_channel)) { // Device sends a program change
    if (patch_number != program) {
      prev_patch_number = patch_number;
      patch_number = program;
      //update_page = REFRESH_PAGE;
      //page_check();
      do_after_patch_selection();
    }
  }
}

void MD_base_class::forward_PC_message(uint8_t program, uint8_t channel) {}

void MD_base_class::check_CC_in(uint8_t control, uint8_t value, uint8_t channel, uint8_t port) {  // Check incoming CC messages from GR-55
  if ((channel == MIDI_channel) && (port == MIDI_port)) {
    if (control == 0) {
      CC00 = value;
      DEBUGMSG("Received CC0 with value " + String(value));
    }
  }
}

void MD_base_class::check_active_sense_in(uint8_t port) {}

void MD_base_class::check_still_connected() { // Started from MIDI/MIDI_check_all_devices_still_connected()
  if ((connected) && (enabled == DEVICE_DETECT)) {
    if (no_response_counter >= max_times_no_response) disconnect();
    no_response_counter++;
  }
}

void MD_base_class::check_manual_connection() {

  if (enabled == DEVICE_ON) { // We always have to set the port, when enabled == DEVICE_ON, so a change of manual port will update the device port
    MIDI_port = MIDI_set_port_number_from_menu(MIDI_port_manual);
    if (!connected) connect(MIDI_device_id, MIDI_port); // We connect only when the device is not connected yet.
  }

  if ((connected) && (enabled == DEVICE_OFF)) disconnect();
}

bool MD_base_class::can_request_sysex_data() {
  if (enabled == DEVICE_DETECT) return connected;
  else return false;
}

void MD_base_class::send_alternative_identity_request(uint8_t check_device_no) {}

void MD_base_class::identity_check(const unsigned char* sxdata, short unsigned int sxlength, uint8_t port) {}

void MD_base_class::connect(uint8_t device_id, uint8_t port) {
  DEBUGMAIN(String(device_name) + " connected on MIDI port " + String(port >> 4) + ':' + String(port & 0x0F));
  connected = true;
  MIDI_device_id = device_id;
  MIDI_port = port; // Set the correct MIDI port for this device
  do_after_connect();
  PAGE_check_first_connect(my_device_number); // Go to the device page of this device if it is the first device that connects
  if (enabled == DEVICE_DETECT) LCD_show_popup_label(String(device_name) + " connected ", MESSAGE_TIMER_LENGTH);
}

void MD_base_class::do_after_connect() {}
void MD_base_class::do_after_disconnect() {}

void MD_base_class::disconnect() {
  connected = false;
  is_on = false;
  current_exp_pedal = 0; // No expression pedal active
  LCD_clear_string(current_patch_name);
  LCD_show_popup_label(String(device_name) + " offline   ", MESSAGE_TIMER_LENGTH);
  PAGE_check_disconnect(my_device_number);
  DEBUGMAIN(String(device_name) + " offline");
  update_page = RELOAD_PAGE;
  update_main_lcd = true;
}

// ********************************* Section 3: Device common MIDI out functions ********************************************

void MD_base_class::check_sysex_delay() { // Will delay if last message was within sysex_delay_length (10 ms)
  if (MIDI_port == USBHMIDI_PORT) return;
  while (millis() - sysexDelay <= sysex_delay_length) {
    //main_MIDI_common(); // Keep the MIDI input detection going. Otherwise we get errors on midi input
  }
  sysexDelay = millis();
}

// Calculate the Roland checksum
uint8_t MD_base_class::calc_Roland_checksum(uint16_t sum) {
  uint8_t checksum = 0x80 - (sum & 0x7F);
  return checksum & 0x7F;
}


void MD_base_class::set_bpm() {}
void MD_base_class::bpm_tap() {}
void MD_base_class::start_tuner() {}
void MD_base_class::stop_tuner() {}

// ********************************* Section 4: Device program change ********************************************

void MD_base_class::select_patch(uint16_t new_patch) {

  if (new_patch == patch_number) unmute();
  prev_patch_number = patch_number;
  patch_number = new_patch;
  MIDI_send_PC(new_patch, MIDI_channel, MIDI_port);
  DEBUGMSG("out(" + String(device_name) + ") PC" + String(new_patch)); //Debug
  do_after_patch_selection();
}

void MD_base_class::do_after_patch_selection() {
  if (!PAGE_check_on_page(my_device_number, patch_number)) { // Check if patch is on the page
    update_page = REFRESH_PAGE;
  }
  else {
    update_page = REFRESH_FX_ONLY;
  }
}

bool MD_base_class::request_patch_name(uint8_t sw, uint16_t number) {
  LCD_clear_SP_label(sw);
  return true;
}

void MD_base_class::request_current_patch_name() {}

uint16_t MD_base_class::calculate_patch_number(uint8_t bank_position, uint8_t bank_size) {
  uint16_t new_patch;
  if (bank_selection_active()) new_patch = (bank_select_number * bank_size) + bank_position;
  else new_patch = (bank_number * bank_size) + bank_position;
  if (new_patch > patch_max) new_patch = new_patch - patch_max - 1;
  return new_patch;
}

bool MD_base_class::patch_select_pressed(uint16_t new_patch) {
  // Check whether the current patch needs to be switched on or whether a new patch is chosen
  if (new_patch > patch_max) new_patch = patch_max;
  if (new_patch == patch_number) { // Check if curnum needs to be executed.
    DEBUGMSG("Executing curnum action");
    switch (Setting.CURNUM_action) {
      case CN_PREV_PATCH:
        if (prev_patch_number != patch_number) patch_select_pressed(prev_patch_number);
        return true;
      case CN_TAP_TEMPO:
        SCO_global_tap_tempo_press(0);
        break;
      case CN_GLOBAL_TUNER:
        SCO_global_tuner_start();
        break;
      case CN_US20_EMULATION:
        select_switch();
        break;
      case CN_DIRECT_SELECT:
        SCO_select_page(PAGE_CURRENT_DIRECT_SELECT);
        break;
    };
    return false;
  }
  else {
    DEBUGMSG("Selecting patch " + String (new_patch));
    select_patch(new_patch); //New patch - send program change
    set_current_device(my_device_number);
    PC_ignore_timer = millis();
    popup_patch_name = true;
  }
  bank_number = bank_select_number; // Update the real bank number with the selected number
  device_in_bank_selection = 0; // Switch off bank selection mode
  return true;
}

uint16_t MD_base_class::calculate_prev_patch_number() {
  if (patch_number > patch_min) return (patch_number - 1);
  else return (patch_max);
}

uint16_t MD_base_class::calculate_next_patch_number() {
  if (patch_number < patch_max) return (patch_number + 1);
  else return (patch_min);
}

void MD_base_class::bank_updown(signed int delta, uint8_t my_bank_size) {

  //uint16_t rel_patch_number = patch_number - patch_min;
  uint16_t rel_patch_max = patch_max - patch_min;
  if (!bank_selection_active()) {
    device_in_bank_selection = my_device_number + 1; // Use of static variable device_in_bank_selection will make sure only one device is in bank select mode.
    bank_select_number = bank_number; //Reset the bank to current patch
    bank_size = my_bank_size;
  }

  /*if (delta > 0) { // Perform bank up:
    for (uint8_t i = 0; i < delta; i++) {
      if (bank_select_number >= (rel_patch_max / bank_size)) bank_select_number = 0; // Check if we've reached the top
      else bank_select_number ++;
    }
  }
  if (delta < 0) { // Perform bank down:
    for (uint8_t i = 0; i < abs(delta); i++) {
      if (bank_select_number <= 0) bank_select_number = (rel_patch_max / bank_size); // Check if we've reached the bottom
      else bank_select_number --;
    }
  }*/
  bank_select_number = update_encoder_value(delta, bank_select_number, 0, (rel_patch_max / bank_size));

  if (bank_select_number == bank_number) device_in_bank_selection = 0; //Check whether were back to the original bank

  request_bank_name(delta, bank_select_number * my_bank_size);
}

bool MD_base_class::bank_selection_active() {
  return (device_in_bank_selection == my_device_number + 1);
}

void MD_base_class::update_bank_number(uint16_t number) {
  uint16_t bnumber;
  if (number > patch_max) number = patch_max;
  if (bank_size == 0) bnumber = 0;
  else bnumber = (number - patch_min) / bank_size;
  bank_number = bnumber;
  bank_select_number = bnumber;
}

void MD_base_class::update_bank_size(uint8_t b_size) { // If the bank size changes, update the current bank number
  if (bank_size != b_size) {
    bank_size = b_size;
    update_bank_number(patch_number);
  }
}

void MD_base_class::request_bank_name(signed int delta, uint16_t number) {}

bool MD_base_class::flash_LEDs_for_patch_bank_switch(uint8_t sw) {
  return bank_selection_active();
}

void MD_base_class::display_patch_number_string() {
  if (bank_selection_active() == false) {
    number_format(patch_number, Current_patch_number_string);
    snapscene_number_format(Current_patch_number_string);
  }
  else {
    String start_number, end_number;
    number_format(bank_select_number * bank_size + patch_min, start_number);
    number_format((bank_select_number + 1) * bank_size - 1 + patch_min, end_number);
    Current_patch_number_string += start_number + "-" + end_number;
  }
}

void MD_base_class::number_format(uint16_t number, String &Output) {
  Output += String((number + 1) / 10) + String((number + 1) % 10);
}

void MD_base_class::direct_select_format(uint16_t number, String &Output) {
  if (direct_select_state == 0) Output += String(number) + "_";
  else Output += String(bank_select_number) + String(number);
}

bool MD_base_class::valid_direct_select_switch(uint8_t number) {
  if (direct_select_state == 0) return ((number * 10) + (bank_select_number * 100) <= patch_max);
  else return true;
}

void MD_base_class::direct_select_start() {
  Previous_bank_size = bank_size; // Remember the bank size
  device_in_bank_selection = my_device_number + 1;
  bank_size = 100;
  bank_select_number = patch_number / bank_size; // Reset bank number
  direct_select_state = 0;
}

uint16_t MD_base_class::direct_select_patch_number_to_request(uint8_t number) {
  if (direct_select_state == 0) return (number * 10) + (bank_select_number * 100);
  else {
    uint16_t new_patch_number = (bank_select_number * 10) + number;
    if (new_patch_number < patch_number_offset) new_patch_number = patch_number_offset;
    return new_patch_number - patch_number_offset;
  }
}

void MD_base_class::direct_select_press(uint8_t number) {
  if (!valid_direct_select_switch(number)) return;
  if (direct_select_state == 0) {
    // First digit pressed
    bank_select_number = number + (bank_select_number * 10);
    bank_size = 10;
    direct_select_state = 1;
  }
  else {
    // Second digit pressed
    device_in_bank_selection = 0;
    uint16_t new_patch = (bank_select_number * 10) + number;
    if (new_patch <= patch_min) new_patch = patch_min + patch_number_offset;
    if (new_patch > patch_max) new_patch = patch_max;
    bank_number = ((new_patch - patch_number_offset) / Previous_bank_size); // Set bank number to the new patch
    bank_size = Previous_bank_size;
    bank_select_number = bank_number;
    select_patch(new_patch - patch_number_offset);
    SCO_select_page(Previous_page);
  }
}

// ** US-20 simulation

void MD_base_class::unmute() {}

void MD_base_class::mute() {}

void MD_base_class::select_switch() {
  if (Current_device == my_device_number) {
    is_always_on_toggle();
  }
  else {
    unmute();
    set_current_device(my_device_number);
    update_main_lcd = true;
    update_LEDS = true;
  }
}

void MD_base_class::is_always_on_toggle() {
  if (US20_mode_enabled()) {
    is_always_on = !is_always_on; // Toggle is_always_on
    if (is_always_on) {
      unmute();
      LCD_show_popup_label(String(device_name) + " always ON", ACTION_TIMER_LENGTH);
    }
    else {
      //mute();
      LCD_show_popup_label(String(device_name) + " can be muted", ACTION_TIMER_LENGTH);
    }
  }
}

bool MD_base_class::US20_mode_enabled() {
  return (Setting.CURNUM_action == CN_US20_EMULATION);
}

// ********************************* Section 5: Effect parameter and assign control ********************************************
void MD_base_class::read_parameter_title(uint16_t number, String &Output) {
  Output += device_name;
}

void MD_base_class::read_parameter_title_short(uint16_t number, String &Output) {
  read_parameter_title(number, Output);
}

void MD_base_class::read_parameter_name(uint16_t number, String &Output) { // Called from menu
  Output = "no parameters";
}

void MD_base_class::read_parameter_value_name(uint16_t number, uint16_t value, String &Output) {
  Output = "-";
}

void MD_base_class::parameter_press(uint8_t Sw, Cmd_struct *cmd, uint16_t number) {}

void MD_base_class::parameter_release(uint8_t Sw, Cmd_struct *cmd, uint16_t number) {}

bool MD_base_class::request_parameter(uint8_t sw, uint16_t number) {
  // Not implemented in base class - move on to next switch
  LCD_clear_SP_label(sw);
  return true;
}

uint16_t MD_base_class::number_of_parameters() {
  return 0;
}

uint8_t MD_base_class::number_of_values(uint16_t parameter) {
  return 0;
}

uint16_t MD_base_class::number_of_parbank_parameters() { // By default the same as number of parameters
  return number_of_parameters();
}

uint16_t MD_base_class::get_parbank_parameter_id(uint16_t par_number) { // By default we want to be able to see all parameters here
  return par_number;
}

void MD_base_class::request_par_bank_category_name(uint8_t sw) {
  LCD_clear_SP_label(sw);
}

void MD_base_class::select_parameter_bank_category(uint8_t category) {
  parameter_bank_category = category;
  parameter_bank_number = 0; // Go to the first bank
}

void MD_base_class::par_bank_updown(signed int delta, uint8_t my_bank_size) {

  /*// Perform bank up:
  if (delta > 0) {
    for (uint8_t i = 0; i < delta; i++) {
      if (parameter_bank_number >= (number_of_parbank_parameters() - 1) / my_bank_size) parameter_bank_number = 0; // Check if we've reached the top
      else parameter_bank_number++; //Otherwise move bank up
    }
  }
  // Perform bank down:
  if (delta < 0) {
    for (uint8_t i = 0; i < abs(delta); i++) {
      if (parameter_bank_number <= 0) parameter_bank_number = (number_of_parbank_parameters() - 1) / my_bank_size; // Check if we've reached the bottom
      else parameter_bank_number--; //Otherwise move bank down
    }
  }*/
  parameter_bank_number = update_encoder_value(delta, parameter_bank_number, 0, number_of_parbank_parameters() - 1);

  if (my_bank_size == 1) { // Show the current parameter name and value if the bank size is only 1
    String msg = "";
    read_parameter_name(parameter_bank_number, msg);
    LCD_show_popup_label(msg, MESSAGE_TIMER_LENGTH);
  }
  else {
    LCD_show_popup_label("Par bank " + String(parameter_bank_number + 1) + "/" + String((number_of_parbank_parameters() - 1) / my_bank_size + 1), ACTION_TIMER_LENGTH);
  }

  update_page = REFRESH_PAGE; //Re-read the patchnames for this bank
}

void MD_base_class::read_assign_name(uint8_t number, String &Output) {
  Output = "no assigns";
}

void MD_base_class::read_assign_short_name(uint8_t number, String &Output) {
  Output = " -- ";
}

void MD_base_class::read_assign_trigger(uint8_t number, String &Output) {
  Output = "-";
}

uint8_t MD_base_class::get_number_of_assigns() {
  return 0;
}

uint8_t MD_base_class::trigger_follow_assign(uint8_t number) {
  return 0;
}

void MD_base_class::assign_press(uint8_t Sw, uint8_t value) {}

void MD_base_class::assign_release(uint8_t Sw) {}

void MD_base_class::assign_load(uint8_t sw, uint8_t assign_number, uint8_t cc_number) {}

void MD_base_class::request_current_assign(uint8_t sw) {
  // Not implemented in base class - move on to next switch
  PAGE_request_next_switch();
}

void MD_base_class::asgn_bank_updown(signed int delta, uint8_t my_bank_size) {

  /*// Perform bank up:
  if (delta > 0) {
    for (uint8_t i = 0; i < delta; i++) {
      if (assign_bank_number >= (get_number_of_assigns() - 1) / my_bank_size) assign_bank_number = 0; // Check if we've reached the top
      else assign_bank_number++; //Otherwise move bank up
    }
  }
  // Perform bank down:
  if (delta < 0) {
    for (uint8_t i = 0; i < abs(delta); i++) {
      if (assign_bank_number <= 0) assign_bank_number = (get_number_of_assigns() - 1) / my_bank_size; // Check if we've reached the bottom
      else assign_bank_number--; //Otherwise move bank down
    }
  }
  else {
    LCD_show_popup_label("Asgn bank " + String(assign_bank_number + 1) + "/" + String((get_number_of_assigns() - 1) / my_bank_size + 1), ACTION_TIMER_LENGTH);
  }*/
  assign_bank_number = update_encoder_value(delta, assign_bank_number, 0, get_number_of_assigns() - 1);
  LCD_show_popup_label("Asgn bank " + String(assign_bank_number + 1) + "/" + String((get_number_of_assigns() - 1) / my_bank_size + 1), ACTION_TIMER_LENGTH);

  update_page = REFRESH_PAGE; //Re-read the patchnames for this bank
}

void MD_base_class::FX_press(uint8_t Sw, Cmd_struct *cmd, uint8_t number) {}  // Method for ZOOM pedals
void MD_base_class::FX_set_type_and_state(uint8_t Sw) {}  // Method for ZOOM pedals

void MD_base_class::move_expression_pedal(uint8_t sw, uint8_t value, uint8_t exp_pedal) {}

void MD_base_class::toggle_expression_pedal(uint8_t sw) {
  if (current_exp_pedal == 0) return;
  current_exp_pedal++;
  if (current_exp_pedal > 2) current_exp_pedal = 1;
  update_page = REFRESH_PAGE;
}

void MD_base_class::set_expr_title(uint8_t sw) {
  const char LCD_Exp0[] = " EXP1  EXP2 ";
  const char LCD_Exp1[] = "[EXP1] EXP2 ";
  const char LCD_Exp2[] = " EXP1 [EXP2]";
  if (current_exp_pedal == 0) LCD_set_SP_title(sw, LCD_Exp0);
  else if (current_exp_pedal == 1) LCD_set_SP_title(sw, LCD_Exp1);
  else LCD_set_SP_title(sw, LCD_Exp2);
}

bool MD_base_class::request_exp_pedal(uint8_t sw, uint8_t exp_pedal) {
  LCD_clear_SP_label(sw);
  return true;
}

uint8_t MD_base_class::read_current_device_page() { // Return the number of the current page
  switch (current_device_page) {
    case 0: return my_device_page1;
    case 1: return my_device_page2;
    case 2: return my_device_page3;
    case 3: return my_device_page4;
    default: return my_device_page1;
  }
}

uint8_t MD_base_class::select_next_device_page() { // Select the next page for this device
  uint8_t tries = 4;

  if (Current_page == read_current_device_page()) { // Select next page unless we are on some other page.
    while (tries > 0) {
      current_device_page++;
      if (current_device_page >= 4) current_device_page = 0;
      if (read_current_device_page() != 0) return read_current_device_page();
      tries--;
    }
  }
  else if (Previous_page != read_current_device_page()) { // Finding our way back from the par-bank edit screen to category to the current_device_page.
    uint8_t pg = Previous_page;
    Current_page = read_current_device_page(); // This will set the previous page on the next page change...
    return pg;
  }
  return read_current_device_page();
}

void MD_base_class::set_snapscene_title(uint8_t number, String &Output) {
  Output += "Not supported";
}

bool MD_base_class::request_snapscene_name(uint8_t sw, uint8_t number) {
  LCD_clear_SP_label(sw);
  return true;
}

/*void MD_base_class::set_snapscene_name(uint8_t number, String &Output) {
  Output = "Not supported";
  }*/

void MD_base_class::set_snapscene(uint8_t number) {}

void MD_base_class::snapscene_number_format(String &Output) {}

// ********************************* Section 6: Looper control ********************************************

bool MD_base_class::looper_active() {
  return false;
}

uint8_t MD_base_class::show_looper_LED(uint8_t sw) {
  if ((!connected) || (!looper_active())) return 0; // Switch all LEDs off when looper is not active

  enum LED_states { L_OFF, L_DIMMED, L_ON, L_FLASHING };
  LED_states state = L_DIMMED;

  uint8_t colour = Setting.FX_LOOPER_colour;

  switch (SP[sw].PP_number) {
    case LOOPER_SHOW_HIDE:
      if (looper_show) state = L_ON;
      break;
    case LOOPER_PLAY_STOP:
      if ((current_looper_state == LOOPER_STATE_PLAY) || (current_looper_state == LOOPER_STATE_PLAY_ONCE) || (current_looper_state == LOOPER_STATE_OVERDUB))  {
        state = L_ON;
      }
      if ((looper_active()) && (loop_recorded)) {
        colour = 1; // Green
      }
      break;
    case LOOPER_REC_OVERDUB:
      if (current_looper_state == LOOPER_STATE_RECORD) {
        state = L_ON;
        colour = 2; // Red
      }
      else if (current_looper_state == LOOPER_STATE_OVERDUB) {
        state = L_FLASHING;
        colour = 4; // Orange
      }
      else if ((current_looper_state == LOOPER_STATE_PLAY) || (current_looper_state == LOOPER_STATE_PLAY_ONCE)) {
        colour = 4; // Orange
      }
      else colour = 2;
      break;
    case LOOPER_UNDO_REDO:
      if (SP[sw].Pressed) state = L_ON;
      break;
    case LOOPER_HALF_SPEED:
      if (looper_half_speed) state = L_ON;
      break;
    case LOOPER_REVERSE:
      if (looper_reverse) state = L_ON;
      break;
    case LOOPER_PLAY_ONCE:
      if (current_looper_state == LOOPER_STATE_PLAY_ONCE) state = L_ON;
      break;
    case LOOPER_PRE_POST: // M13 supports this...
      if (looper_pre) state = L_ON;
      break;
    case LOOPER_REC_PLAY_OVERDUB:
      if ((current_looper_state == LOOPER_STATE_PLAY) || (current_looper_state == LOOPER_STATE_PLAY_ONCE)) {
        state = L_ON;
        colour = 1; // Green
      }
      if (current_looper_state == LOOPER_STATE_STOPPED) {
        colour = 1; // Green
      }
      if (current_looper_state == LOOPER_STATE_RECORD) {
        state = L_ON;
        colour = 2; // Red
      }
      if (current_looper_state == LOOPER_STATE_OVERDUB) {
        state = L_FLASHING;
        colour = 4; // Orange
      }
      break;
    case LOOPER_STOP_ERASE:
      if ((current_looper_state == LOOPER_STATE_STOPPED) || (current_looper_state == LOOPER_STATE_ERASED)) state = L_ON;
      if (looper_stop_pressed == 2) state = L_FLASHING;
      break;
  }
  if (state == L_DIMMED) return colour | LED_DIMMED;
  if (state == L_ON) return colour;
  if (state == L_FLASHING) return colour | LED_FLASHING;
  return 0;
}

uint8_t MD_base_class::request_looper_backlight_colour() {
  if (!looper_active()) return 0;
  if ((current_looper_state == LOOPER_STATE_PLAY) || (current_looper_state == LOOPER_STATE_PLAY_ONCE)) return 1; // Green
  if (current_looper_state == LOOPER_STATE_RECORD) return 2; // Red
  if (current_looper_state == LOOPER_STATE_OVERDUB) return 4; // Orange
  return 0;
}

void MD_base_class::request_looper_label(uint8_t sw) {
  if ((!connected) || (!looper_active())) { // Clear the labels when looper is not ative
    LCD_clear_SP_label(sw);
    return;
  }

#ifdef IS_VCMINI
  switch (SP[sw].PP_number) {
    case LOOPER_SHOW_HIDE:
      if (looper_show) LCD_set_SP_label(sw, "SHOW");
      else LCD_set_SP_label(sw, "HIDE");
      break;
    case LOOPER_PLAY_STOP:
      if (current_looper_state == LOOPER_STATE_STOPPED) {
        if (loop_recorded) LCD_set_SP_label(sw, "PLAY");
        else LCD_set_SP_label(sw, "STOP");
      }
      if (current_looper_state == LOOPER_STATE_RECORD) LCD_set_SP_label(sw,  "PLAY");
      if (current_looper_state == LOOPER_STATE_OVERDUB) LCD_set_SP_label(sw, "STOP ");
      if (current_looper_state == LOOPER_STATE_PLAY) LCD_set_SP_label(sw, "STOP");
      if (current_looper_state == LOOPER_STATE_PLAY_ONCE) LCD_set_SP_label(sw, "STOP");
      //my_looper_lcd = sw;
      break;
    case LOOPER_REC_OVERDUB:
      if (current_looper_state == LOOPER_STATE_STOPPED) LCD_set_SP_label(sw, "REC");
      if (current_looper_state == LOOPER_STATE_RECORD) LCD_set_SP_label(sw,  "DUB");
      if (current_looper_state == LOOPER_STATE_OVERDUB) LCD_set_SP_label(sw, "PLAY");
      if ((current_looper_state == LOOPER_STATE_PLAY) || (current_looper_state == LOOPER_STATE_PLAY_ONCE)) LCD_set_SP_label(sw, "DUB");
      break;
    case LOOPER_UNDO_REDO:
      if (looper_undone) LCD_set_SP_label(sw, "REDO");
      else LCD_set_SP_label(sw, "UNDO");
      break;
    case LOOPER_HALF_SPEED:
      if (looper_half_speed) LCD_set_SP_label(sw, "HALF");
      else LCD_set_SP_label(sw, "FULL");
      break;
    case LOOPER_REVERSE:
      if (looper_reverse) LCD_set_SP_label(sw, "REV");
      else LCD_set_SP_label(sw, "FWD");
      break;
    case LOOPER_PLAY_ONCE:
      LCD_set_SP_label(sw, "ONCE");
      break;
    case LOOPER_PRE_POST: // M13 supports this...
      if (looper_pre) LCD_set_SP_label(sw, "PRE");
      else LCD_set_SP_label(sw, "POST");
      break;
    case LOOPER_REC_PLAY_OVERDUB:
      if (current_looper_state == LOOPER_STATE_ERASED) LCD_set_SP_label(sw, "REC");
      if (current_looper_state == LOOPER_STATE_RECORD) LCD_set_SP_label(sw,  "PLAY");
      if (current_looper_state == LOOPER_STATE_PLAY) LCD_set_SP_label(sw, "DUB");
      if (current_looper_state == LOOPER_STATE_STOPPED) LCD_set_SP_label(sw, "PLAY");
      if (current_looper_state == LOOPER_STATE_PLAY_ONCE) LCD_set_SP_label(sw, "DUB");
      if (current_looper_state == LOOPER_STATE_OVERDUB) LCD_set_SP_label(sw, "PLAY ");
      //my_looper_lcd = sw;
      break;
    case LOOPER_STOP_ERASE:
      if (current_looper_state == LOOPER_STATE_ERASED) LCD_set_SP_label(sw, "EMPTY");
      else if (current_looper_state != LOOPER_STATE_STOPPED) LCD_set_SP_label(sw, "STOP ");
      else if (looper_stop_pressed != 2) LCD_set_SP_label(sw, "[STOP]");
      else LCD_set_SP_label(sw, "ERASE");
      break;
  }

#else // Regular VController

  switch (SP[sw].PP_number) {
    case LOOPER_SHOW_HIDE:
      if (looper_show) LCD_set_SP_label(sw, "[SHOW] HIDE ");
      else LCD_set_SP_label(sw, " SHOW [HIDE]");
      break;
    case LOOPER_PLAY_STOP:
      if (current_looper_state == LOOPER_STATE_STOPPED) {
        if (loop_recorded) LCD_set_SP_label(sw, " PLAY [STOP]");
        else LCD_set_SP_label(sw, "[STOP]");
      }
      if (current_looper_state == LOOPER_STATE_RECORD) LCD_set_SP_label(sw,  "PLAY");
      if (current_looper_state == LOOPER_STATE_OVERDUB) LCD_set_SP_label(sw, "STOP ");
      if (current_looper_state == LOOPER_STATE_PLAY) LCD_set_SP_label(sw, "[PLAY] STOP");
      if (current_looper_state == LOOPER_STATE_PLAY_ONCE) LCD_set_SP_label(sw, "[ONCE] STOP");
#ifndef IS_VCMINI
      my_looper_lcd = sw;
#endif
      break;
    case LOOPER_REC_OVERDUB:
      if (current_looper_state == LOOPER_STATE_STOPPED) LCD_set_SP_label(sw, "REC");
      if (current_looper_state == LOOPER_STATE_RECORD) LCD_set_SP_label(sw,  "[REC] OVERDUB");
      if (current_looper_state == LOOPER_STATE_OVERDUB) LCD_set_SP_label(sw, "[OVERDUB] PLAY ");
      if ((current_looper_state == LOOPER_STATE_PLAY) || (current_looper_state == LOOPER_STATE_PLAY_ONCE)) LCD_set_SP_label(sw, "OVERDUB");
      break;
    case LOOPER_UNDO_REDO:
      if (looper_undone) LCD_set_SP_label(sw, "REDO");
      else LCD_set_SP_label(sw, "UNDO");
      break;
    case LOOPER_HALF_SPEED:
      if (looper_half_speed) LCD_set_SP_label(sw, " FULL [HALF]");
      else LCD_set_SP_label(sw, "[FULL] HALF ");
      break;
    case LOOPER_REVERSE:
      if (looper_reverse) LCD_set_SP_label(sw, " FWD [REV]");
      else LCD_set_SP_label(sw, "[FWD] REV ");
      break;
    case LOOPER_PLAY_ONCE:
      LCD_set_SP_label(sw, "PLAY ONCE");
      break;
    case LOOPER_PRE_POST: // M13 supports this...
      if (looper_pre) LCD_set_SP_label(sw, "[PRE] POST ");
      else LCD_set_SP_label(sw, " PRE [POST]");
      break;
    case LOOPER_REC_PLAY_OVERDUB:
      if (current_looper_state == LOOPER_STATE_ERASED) LCD_set_SP_label(sw, "REC");
      if (current_looper_state == LOOPER_STATE_RECORD) LCD_set_SP_label(sw,  "[REC] PLAY");
      if (current_looper_state == LOOPER_STATE_PLAY) LCD_set_SP_label(sw, "OVERDUB [PLAY]");
      if (current_looper_state == LOOPER_STATE_STOPPED) LCD_set_SP_label(sw, "PLAY");
      if (current_looper_state == LOOPER_STATE_PLAY_ONCE) LCD_set_SP_label(sw, "OVERDUB [ONCE]");
      if (current_looper_state == LOOPER_STATE_OVERDUB) LCD_set_SP_label(sw, "[OVERDUB] PLAY ");
#ifndef IS_VCMINI
      my_looper_lcd = sw;
#endif
      break;
    case LOOPER_STOP_ERASE:
      if (current_looper_state == LOOPER_STATE_ERASED) LCD_set_SP_label(sw, "[EMPTY]");
      else if (current_looper_state != LOOPER_STATE_STOPPED) LCD_set_SP_label(sw, "STOP ");
      else if (looper_stop_pressed != 2) LCD_set_SP_label(sw, "[STOP]");
      else LCD_set_SP_label(sw, "ERASE");
      break;
  }
#endif
}

void MD_base_class::looper_press(uint8_t looper_cmd, bool send_cmd) {
  if ((!connected) || (!looper_active())) return; // exit if device is not connected or looper is not active
  uint32_t current_time = micros();
  uint32_t prev_end_time;
  switch (looper_cmd) {
    case LOOPER_SHOW_HIDE:
      looper_show ^= 1; // Toggle looper show.
      if (looper_show) send_looper_cmd(LOOPER_CMD_SHOW);
      else send_looper_cmd(LOOPER_CMD_HIDE);
      break;
    case LOOPER_PLAY:
      if (current_looper_state == LOOPER_STATE_PLAY) break; // Exit if already playing
      if (send_cmd) send_looper_cmd(LOOPER_CMD_PLAY);
      if (current_looper_state == LOOPER_STATE_RECORD) { // Check if we just came from record...
        current_loop_length = (current_time - looper_start_time); // Store the length of the loop
        looper_start_time = current_time;
        looper_end_time = current_time + current_loop_length; // Start showing the looper bar
      }
      current_looper_state = LOOPER_STATE_PLAY;
      looper_start_time = current_time;
      looper_end_time = current_time + current_loop_length; // Start showing the looper bar
      break;
    case LOOPER_STOP:
      current_looper_state = LOOPER_STATE_STOPPED;
      if (send_cmd) send_looper_cmd(LOOPER_CMD_STOP);
      looper_end_time = 0; // Will stop the looper bar
      break;
    case LOOPER_PLAY_STOP:
      if (!loop_recorded) break; // Quit if no loop has been recorded yet.
      if ((current_looper_state == LOOPER_STATE_STOPPED) || (current_looper_state == LOOPER_STATE_RECORD)) {
        looper_press(LOOPER_PLAY, send_cmd);
      }
      else {
        looper_press(LOOPER_STOP, send_cmd);
      }
      break;
    case LOOPER_REC:
      if (current_looper_state == LOOPER_STATE_RECORD) break; // Exit if already recording
      current_looper_state = LOOPER_STATE_RECORD;
      if (send_cmd) send_looper_cmd(LOOPER_CMD_REC);
      if (max_looper_length > 0) { // If looper length is set in the init() of the device, we can show the looper bar.
        if (!looper_half_speed) current_loop_length = max_looper_length;
        else current_loop_length = max_looper_length * 2;
        looper_start_time = current_time;
        looper_end_time = looper_start_time + current_loop_length; // Start showing the looper bar
      }
      loop_recorded = true;
      looper_overdub_happened = false;
      looper_undone = false;
      break;
    case LOOPER_OVERDUB:
      if (current_looper_state == LOOPER_STATE_OVERDUB) break; // Exit if already in overdub
      if (send_cmd) send_looper_cmd(LOOPER_CMD_OVERDUB);
      if (current_looper_state == LOOPER_STATE_RECORD) {
        current_loop_length = (current_time - looper_start_time); // Store the length of the loop
        looper_start_time = current_time;
        looper_end_time = current_time + current_loop_length; // Start showing the looper bar
      }
      if (current_looper_state == LOOPER_STATE_OVERDUB) { // Toggle between overdub and play state
        current_looper_state = LOOPER_STATE_PLAY;
      }
      else {
        current_looper_state = LOOPER_STATE_OVERDUB;
        looper_overdub_happened = true;
      }
      break;
    case LOOPER_REC_OVERDUB:
      looper_stop_pressed = 0;
      if (current_looper_state == LOOPER_STATE_STOPPED) { // If we have stopped, start a new recording
        looper_press(LOOPER_REC, send_cmd);
      }
      else if (current_looper_state == LOOPER_STATE_RECORD) {
        current_looper_state = LOOPER_STATE_OVERDUB;
        if (send_cmd) send_looper_cmd(LOOPER_CMD_OVERDUB);
        current_loop_length = (current_time - looper_start_time); // Store the length of the loop
        looper_start_time = current_time;
        looper_end_time = current_time + current_loop_length; // Start showing the looper bar
        looper_overdub_happened = true;
      }
      else if (current_looper_state == LOOPER_STATE_OVERDUB) {
        current_looper_state = LOOPER_STATE_PLAY;
        if (send_cmd) send_looper_cmd(LOOPER_CMD_PLAY);
      }
      else if (current_looper_state == LOOPER_STATE_PLAY) { // Else toggle between overdub and play
        current_looper_state = LOOPER_STATE_OVERDUB;
        if (send_cmd) send_looper_cmd(LOOPER_CMD_OVERDUB);
        looper_overdub_happened = true;
      }
      break;
    case LOOPER_UNDO_REDO:
      if (!looper_overdub_happened) break;
      looper_undone ^= 1; // Toggle looper_undone
      if ((looper_undone) && (send_cmd)) send_looper_cmd(LOOPER_CMD_UNDO);
      else if (send_cmd) send_looper_cmd(LOOPER_CMD_REDO);
      break;
    case LOOPER_HALF_SPEED:
      if (current_looper_state == LOOPER_STATE_RECORD) break; // Can't do this while recording.
      looper_half_speed ^= 1; // Toggle looper_half_speed
      if (looper_half_speed) {
        if (send_cmd) send_looper_cmd(LOOPER_CMD_HALF_SPEED);
        current_loop_length *= 2; // Double the current loop length
        if (current_looper_state != LOOPER_STATE_STOPPED)  {
          looper_end_time += (looper_end_time - current_time); // Add the remaining time to looper_end_time
          looper_start_time = looper_end_time - current_loop_length; // So the looper does not get confused on reversing on half speed
        }
      }
      else {
        if (send_cmd) send_looper_cmd(LOOPER_CMD_FULL_SPEED);
        current_loop_length /= 2; // Half the current loop time
        if (current_looper_state != LOOPER_STATE_STOPPED) {
          looper_end_time -= (looper_end_time - current_time) / 2; // Substract half the remaining time to looper_end_time
          looper_start_time = looper_end_time - current_loop_length; // So the looper does not get confused on reversing on double speed
        }
      }
      break;
    case LOOPER_REVERSE:
      if (current_looper_state == LOOPER_STATE_RECORD) break; // Can't do this while recording.
      looper_reverse ^= 1; // Toggle looper_reverse
      if (looper_reverse) {
        if (send_cmd) send_looper_cmd(LOOPER_CMD_REVERSE);
      }
      else {
        if (send_cmd) send_looper_cmd(LOOPER_CMD_FORWARD);
      }
      if (current_looper_state != LOOPER_STATE_STOPPED) {
        prev_end_time = looper_end_time;
        looper_end_time = current_time + (current_time - looper_start_time); // Toggle remaining time with time already used
        looper_start_time = current_time - (prev_end_time - current_time);
      }
      break;
    case LOOPER_PLAY_ONCE:
      if (!loop_recorded) break; // Quit if no loop has been recorded yet.
      if (send_cmd) send_looper_cmd(LOOPER_CMD_PLAY_ONCE);
      if (current_looper_state == LOOPER_STATE_RECORD) current_loop_length = (current_time - looper_start_time); // Store the length of the loop
      if ((current_looper_state == LOOPER_STATE_RECORD) || (current_looper_state == LOOPER_STATE_STOPPED) || (current_looper_state == LOOPER_STATE_PLAY_ONCE)) {
        looper_start_time = current_time;
        looper_end_time = current_time + current_loop_length; // Reset and show the looper bar
      }
      current_looper_state = LOOPER_STATE_PLAY_ONCE;
      looper_stop_pressed = 0;
      break;
    case LOOPER_PRE_POST:
      looper_pre ^= 1; // Toggle looper_pre
      if (looper_pre) {
        if (send_cmd) send_looper_cmd(LOOPER_CMD_POST);
      }
      else {
        if (send_cmd) send_looper_cmd(LOOPER_CMD_PRE);
      }
      break;
    case LOOPER_REC_PLAY_OVERDUB:
      looper_stop_pressed = 0;
      if (current_looper_state == LOOPER_STATE_ERASED) { // If we have no current loop, start a new recording
        looper_press(LOOPER_REC, send_cmd);
      }
      else if (current_looper_state == LOOPER_STATE_RECORD) {
        current_looper_state = LOOPER_STATE_PLAY;
        if (send_cmd) send_looper_cmd(LOOPER_CMD_PLAY);
        current_loop_length = (current_time - looper_start_time); // Store the length of the loop
        looper_start_time = current_time;
        looper_end_time = looper_start_time + current_loop_length; // Reset and show the looper bar
        looper_overdub_happened = true;
      }
      else if (current_looper_state == LOOPER_STATE_PLAY) { // Else toggle between  overdub and play
        current_looper_state = LOOPER_STATE_OVERDUB;
        if (send_cmd) send_looper_cmd(LOOPER_CMD_OVERDUB);
        looper_overdub_happened = true;
      }
      else {
        if (current_looper_state != LOOPER_STATE_OVERDUB) looper_end_time = current_time + current_loop_length; // Reset and show the looper bar
        current_looper_state = LOOPER_STATE_PLAY;
        if (send_cmd) send_looper_cmd(LOOPER_CMD_PLAY);
      }
      break;
    case LOOPER_STOP_ERASE:
      if (!loop_recorded) break; // Quit if no loop has been recorded yet.
      if (current_looper_state == LOOPER_STATE_RECORD) {
        current_loop_length = current_time - looper_start_time;
        looper_start_time = current_time;
      }
      if (looper_stop_pressed < 2) current_looper_state = LOOPER_STATE_STOPPED;
      else {
        current_looper_state = LOOPER_STATE_ERASED; // KPA does loop erase on third press of stop button
        loop_recorded = false;
      }
      if (send_cmd) send_looper_cmd(LOOPER_CMD_STOP);
      looper_end_time = 0; // Will stop the looper bar
      looper_stop_pressed++;
      break;
  }
}

void MD_base_class::looper_release() {}

void MD_base_class::looper_reset() {
  current_looper_state = LOOPER_STATE_STOPPED;
  send_looper_cmd(LOOPER_CMD_STOP);
  looper_end_time = 0; // Will stop the looper bar
  loop_recorded = false;
  looper_reverse = false;
  looper_half_speed = false;
  looper_overdub_happened = false;
}

uint8_t looper_bar = 0;

void MD_base_class::looper_timer_check() {
  if (looper_end_time > 0) {
    if (micros() > looper_end_time) { // Check if looper timer expires
      looper_start_time = looper_end_time;
      looper_end_time += current_loop_length; // Update the timer - add the loop time to it.

      if (current_looper_state == LOOPER_STATE_PLAY_ONCE) { // Check if play once has finished.
        current_looper_state = LOOPER_STATE_STOPPED;
        send_looper_cmd(LOOPER_CMD_STOP); // So also devices that do not support play once (like KPA) can do this - other devices will stop by themselves
        looper_release(); // Will send the release command for KPA
        looper_stop_pressed++;
        looper_end_time = 0;
        update_page = REFRESH_PAGE;
      }
      if (current_looper_state == LOOPER_STATE_RECORD) { // Loop is full and timed out. Loop will go into play (at least it does on the Helix...)
        current_looper_state = LOOPER_STATE_PLAY;
        update_page = REFRESH_PAGE;
      }
    }
    else { // Timer not expired and looper_lcd active
      // Show looper bar
      uint64_t long_end_time = looper_end_time; // Have to calculate the bar using 64 bit numbers, otherwise we get errors on longer loops
      uint64_t long_current_time = micros();
      uint64_t long_length = current_loop_length;
      uint8_t new_bar = (long_end_time - long_current_time) * 128 / long_length;
      if ((looper_bar != new_bar) && (new_bar < 128)) { // Check if this is a new looper bar length
        looper_bar = new_bar;
        //LCD_set_looper_title(); // So the display will match
        if (!looper_reverse) LCD_show_bar(my_looper_lcd, 127 - looper_bar);
        else  LCD_show_bar(my_looper_lcd, looper_bar);
      }
    }
  }
}

void MD_base_class::send_looper_cmd(uint8_t cmd) {}
