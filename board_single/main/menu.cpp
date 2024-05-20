extern "C"{
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "ssd1306.h"
}

#include "menu.hpp"
#include "encoder.hpp"
#include "motorctl.hpp"


//--- variables ---
static const char *TAG = "menu";
static menuState_t menuState = MAIN_MENU;
static int value = 0;



//================================
//===== CONFIGURE MENU ITEMS =====
//================================
// Instructions / Behavior:
// - when line4 * and line5 * are empty the value is printed large
// - when 3rd element is not NULL (pointer to defaultValue function) return int value of that function is shown in line 2
// - when 2nd element is NULL (pointer to currentValue function): instead of current value "click to confirm is shown" in line 3

//#########################
//#### center Joystick ####
//#########################
void item_centerJoystick_action(display_task_parameters_t * objects, SSD1306_t * display, int value){
    ESP_LOGW(TAG, "defining joystick center");
    objects->joystick->defineCenter();
    objects->buzzer->beep(3, 60, 40);
}
menuItem_t item_centerJoystick = {
    item_centerJoystick_action, // function action
    NULL,                       // function get initial value or NULL(show in line 2)
    NULL,                       // function get default value or NULL(dont set value, show msg)
    0,                          // valueMin
    0,                          // valueMax
    0,                          // valueIncrement
    "Center Joystick ",          // title
    "Center Joystick ",          // line1 (above value)
    "",                         // line2 (above value)
    "defines current ",          // line4 * (below value)
    "pos as center   ",            // line5 *
    "",                         // line6
    "=>long to cancel",           // line7
};

// ############################
// #### calibrate Joystick ####
// ############################
// continously show/update joystick data on display
#define CALIBRATE_JOYSTICK_UPDATE_INTERVAL 50
void item_calibrateJoystick_action(display_task_parameters_t *objects, SSD1306_t *display, int value)
{
    //--- variables ---
    bool running = true;
    joystickCalibrationMode_t mode = X_MIN;
    rotary_encoder_event_t event;
    int valueNow = 0;

    //-- pre loop instructions --
    ESP_LOGW(TAG, "starting joystick calibration sequence");
    ssd1306_clear_screen(display, false);

    //-- show static lines --
    // show first line (title)
    displayTextLine(display, 0, false, true, "calibrate stick");
    // show last line (info)
    displayTextLineCentered(display, 7, false, true, " click: confirm ");
    // show initital state
    displayTextLineCentered(display, 1, true, false, "%s", "X-min");

    //-- loop until all positions are defined --
    while (running && objects->control->getCurrentMode() == controlMode_t::MENU)
    {
        // repeatedly print adc value depending on currently selected axis
        switch (mode)
        {
        case X_MIN:
        case X_MAX:
            displayTextLineCentered(display, 4, true, false, "%d", valueNow = objects->joystick->getRawX()); // large
            break;
        case Y_MIN:
        case Y_MAX:
            displayTextLineCentered(display, 4, true, false, "%d", valueNow = objects->joystick->getRawY()); // large
            break;
        case X_CENTER:
        case Y_CENTER:
            displayTextLine(display, 4, false, false, "    x = %d", objects->joystick->getRawX());
            displayTextLine(display, 5, false, false, "    y = %d", objects->joystick->getRawY());
            displayTextLine(display, 6, false, false, "release & click!");
            break;
        }

        // handle encoder event
        // save and next when button clicked, exit when long pressed
        if (xQueueReceive(objects->encoderQueue, &event, CALIBRATE_JOYSTICK_UPDATE_INTERVAL / portTICK_PERIOD_MS))
        {
            objects->control->resetTimeout(); // user input -> reset switch to IDLE timeout
            switch (event.type)
            {
            case RE_ET_BTN_CLICKED:
                objects->buzzer->beep(2, 120, 50);
                switch (mode)
                {
                case X_MIN:
                    // save x min position
                    ESP_LOGW(TAG, "calibrate-stick: saving  X_MIN");
                    objects->joystick->writeCalibration(mode, valueNow);
                    displayTextLineCentered(display, 1, true, false, "%s", "X-max");
                    mode = X_MAX;
                    break;
                case X_MAX:
                    // save x max position
                    ESP_LOGW(TAG, "calibrate-stick: saving  X_MAX");
                    objects->joystick->writeCalibration(mode, valueNow);
                    displayTextLineCentered(display, 1, true, false, "%s", "Y-min");
                    mode = Y_MIN;
                    break;
                case Y_MIN:
                    // save y min position
                    ESP_LOGW(TAG, "calibrate-stick: saving  Y_MIN");
                    objects->joystick->writeCalibration(mode, valueNow);
                    displayTextLineCentered(display, 1, true, false, "%s", "Y-max");
                    mode = Y_MAX;
                    break;
                case Y_MAX:
                    // save y max position
                    ESP_LOGW(TAG, "calibrate-stick: saving  Y_MAX");
                    objects->joystick->writeCalibration(mode, valueNow);
                    displayTextLineCentered(display, 1, true, false, "%s", "CENTR");
                    mode = X_CENTER;
                    break;
                case X_CENTER:
                case Y_CENTER:
                    // save center position
                    ESP_LOGW(TAG, "calibrate-stick: saving  CENTER -> finished");
                    objects->joystick->defineCenter();
                    // finished
                    running = false;
                    break;
                }
                break;
            case RE_ET_BTN_LONG_PRESSED:
                //exit to main-menu
                objects->buzzer->beep(1, 1000, 10);
                ESP_LOGW(TAG, "aborting calibration sqeuence");
                running = false;
            case RE_ET_CHANGED:
            case RE_ET_BTN_PRESSED:
            case RE_ET_BTN_RELEASED:
                break;
            }
        }
    }
}

menuItem_t item_calibrateJoystick = {
    item_calibrateJoystick_action, // function action
    NULL,                          // function get initial value or NULL(show in line 2)
    NULL,                          // function get default value or NULL(dont set value, show msg)
    0,                             // valueMin
    0,                             // valueMax
    0,                             // valueIncrement
    "Calibrate Stick ",            // title
    "   Calibrate    ",            // line1 (above value)
    "   Joystick     ",            // line2 (above value)
    " click to start ",            // line4 * (below value)
    "   sequence     ",            // line5 *
    "                ",            // line6
    "=>long to cancel",            // line7
};


//########################
//#### debug Joystick ####
//########################
//continously show/update joystick data on display
#define DEBUG_JOYSTICK_UPDATE_INTERVAL 50
void item_debugJoystick_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    //--- variables ---
    bool running = true;
    rotary_encoder_event_t event;

    //-- pre loop instructions --
    ESP_LOGW(TAG, "showing joystick debug page");
    ssd1306_clear_screen(display, false);
    // show title
    displayTextLine(display, 0, false, true, " - debug stick - ");
    // show info line
    displayTextLineCentered(display, 7, false, true, "click to exit");

    //-- show/update values --
    // stop when button pressed or control state changes (timeouts to IDLE)
    while (running && objects->control->getCurrentMode() == controlMode_t::MENU)
    {
        // repeatedly print all joystick data
        joystickData_t data = objects->joystick->getData();
        displayTextLine(display, 1, false, false, "x = %.3f     ", data.x);
        displayTextLine(display, 2, false, false, "y = %.3f     ", data.y);
        displayTextLine(display, 3, false, false, "radius = %.3f", data.radius);
        displayTextLine(display, 4, false, false, "angle = %-06.3f   ", data.angle);
        displayTextLine(display, 5, false, false, "pos=%-12s ", joystickPosStr[(int)data.position]);

        // exit when button pressed
        if (xQueueReceive(objects->encoderQueue, &event, DEBUG_JOYSTICK_UPDATE_INTERVAL / portTICK_PERIOD_MS))
        {
            objects->control->resetTimeout(); // user input -> reset switch to IDLE timeout
            switch (event.type)
            {
            case RE_ET_BTN_CLICKED:
            case RE_ET_BTN_LONG_PRESSED:
                running = false;
                objects->buzzer->beep(1, 100, 10);
                break;
            case RE_ET_CHANGED:
            case RE_ET_BTN_PRESSED:
            case RE_ET_BTN_RELEASED:
                break;
            }
        }
    }
}

menuItem_t item_debugJoystick = {
    item_debugJoystick_action, // function action
    NULL,                      // function get initial value or NULL(show in line 2)
    NULL,                      // function get default value or NULL(dont set value, show msg)
    0,                         // valueMin
    0,                         // valueMax
    0,                         // valueIncrement
    "Debug joystick  ",        // title
    "Debug joystick  ",        // line1 (above value)
    "",                        // line2 (above value)
    "",                        // line4 * (below value)
    "debug screen    ",        // line5 *
    "prints values   ",        // line6
    "=>long to cancel",        // line7
};


//########################
//##### set max duty #####
//########################
void maxDuty_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    objects->control->setMaxDuty(value);
}
int maxDuty_currentValue(display_task_parameters_t * objects)
{
    return (int)objects->control->getMaxDuty();
}
menuItem_t item_maxDuty = {
    maxDuty_action,       // function action
    maxDuty_currentValue, // function get initial value or NULL(show in line 2)
    NULL,                 // function get default value or NULL(dont set value, show msg)
    1,                    // valueMin
    100,                  // valueMax
    1,                    // valueIncrement
    "Set max Duty    ",   // title
    "",                   // line1 (above value)
    "  set max-duty: ",   // line2 (above value)
    "",                   // line4 * (below value)
    "",                   // line5 *
    "      1-100     ",   // line6
    "     percent    ",   // line7
};


//##################################
//##### set max relative boost #####
//##################################
void maxRelativeBoost_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    objects->control->setMaxRelativeBoostPer(value);
}
int maxRelativeBoost_currentValue(display_task_parameters_t * objects)
{
    return (int)objects->control->getMaxRelativeBoostPer();
}
menuItem_t item_maxRelativeBoost = {
    maxRelativeBoost_action,       // function action
    maxRelativeBoost_currentValue, // function get initial value or NULL(show in line 2)
    NULL,                 // function get default value or NULL(dont set value, show msg)
    0,                    // valueMin
    150,                  // valueMax
    1,                    // valueIncrement
    "Set max Boost   ",   // title
    "Set max Boost % ",                   // line1 (above value)
    "for outer tire  ",   // line2 (above value)
    "",                   // line4 * (below value)
    "",                   // line5 *
    "  % of max duty ",   // line6
    "added on turning",   // line7
};


//######################
//##### accelLimit #####
//######################
void item_accelLimit_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    objects->motorLeft->setFade(fadeType_t::ACCEL, (uint32_t)value);
    objects->motorRight->setFade(fadeType_t::ACCEL, (uint32_t)value);
}
int item_accelLimit_value(display_task_parameters_t * objects)
{
    return objects->motorLeft->getFade(fadeType_t::ACCEL);
}
int item_accelLimit_default(display_task_parameters_t * objects)
{
    return objects->motorLeft->getFadeDefault(fadeType_t::ACCEL);
}
menuItem_t item_accelLimit = {
    item_accelLimit_action,  // function action
    item_accelLimit_value,   // function get initial value or NULL(show in line 2)
    item_accelLimit_default, // function get default value or NULL(dont set value, show msg)
    0,                       // valueMin
    10000,                   // valueMax
    100,                     // valueIncrement
    "Accel limit     ",      // title
    " Fade up time   ",      // line1 (above value)
    "",                      // line2 <= showing "default = %d"
    "",                      // line4 * (below value)
    "",                      // line5 *
    "milliseconds    ",      // line6
    "from 0 to 100%  ",      // line7
};


// ######################
// ##### decelLimit #####
// ######################
void item_decelLimit_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    objects->motorLeft->setFade(fadeType_t::DECEL, (uint32_t)value);
    objects->motorRight->setFade(fadeType_t::DECEL, (uint32_t)value);
}
int item_decelLimit_value(display_task_parameters_t * objects)
{
    return objects->motorLeft->getFade(fadeType_t::DECEL);
}
int item_decelLimit_default(display_task_parameters_t * objects)
{
    return objects->motorLeft->getFadeDefault(fadeType_t::DECEL);
}
menuItem_t item_decelLimit = {
    item_decelLimit_action,  // function action
    item_decelLimit_value,   // function get initial value or NULL(show in line 2)
    item_decelLimit_default, // function get default value or NULL(dont set value, show msg)
    0,                       // valueMin
    10000,                   // valueMax
    100,                     // valueIncrement
    "Decel limit     ",      // title
    " Fade down time ",      // line1 (above value)
    "",                      // line2 <= showing "default = %d"
    "",                      // line4 * (below value)
    "",                      // line5 *
    "milliseconds    ",      // line6
    "from 100 to 0%  ",      // line7
};


// ######################
// ##### brakeDecel #####
// ######################
void item_brakeDecel_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    objects->motorLeft->setBrakeDecel((uint32_t)value);
    objects->motorRight->setBrakeDecel((uint32_t)value);
}
int item_brakeDecel_value(display_task_parameters_t * objects)
{
    return objects->motorLeft->getBrakeDecel();
}
int item_brakeDecel_default(display_task_parameters_t * objects)
{
    return objects->motorLeft->getBrakeDecelDefault();
}
menuItem_t item_brakeDecel = {
    item_brakeDecel_action,  // function action
    item_brakeDecel_value,   // function get initial value or NULL(show in line 2)
    item_brakeDecel_default, // function get default value or NULL(dont set value, show msg)
    0,                       // valueMin
    10000,                   // valueMax
    100,                     // valueIncrement
    "Brake decel.    ",      // title
    " Fade down time ",      // line1 (above value)
    "",                      // line2 <= showing "default = %d"
    "",                      // line4 * (below value)
    "",                      // line5 *
    "milliseconds    ",      // line6
    "from 100 to 0%  ",      // line7
};


//###############################
//### select motorControlMode ###
//###############################
void item_motorControlMode_action(display_task_parameters_t *objects, SSD1306_t *display, int value)
{
    switch (value)
    {
    case 1:
    default:
    objects->motorLeft->setControlMode(motorControlMode_t::DUTY);
    objects->motorRight->setControlMode(motorControlMode_t::DUTY);
        break;
    case 2:
    objects->motorLeft->setControlMode(motorControlMode_t::CURRENT);
    objects->motorRight->setControlMode(motorControlMode_t::CURRENT);
        break;
    case 3:
    objects->motorLeft->setControlMode(motorControlMode_t::SPEED);
    objects->motorRight->setControlMode(motorControlMode_t::SPEED);
        break;
    }
}
int item_motorControlMode_value(display_task_parameters_t *objects)
{
    return 1; // initial value shown / changed from //TODO get actual mode
}
menuItem_t item_motorControlMode = {
    item_motorControlMode_action, // function action
    item_motorControlMode_value,  // function get initial value or NULL(show in line 2)
    NULL,                     // function get default value or NULL(dont set value, show msg)
    1,                        // valueMin
    3,                        // valueMax
    1,                        // valueIncrement
    "Control mode    ",       // title
    "  sel. motor    ",       // line1 (above value)
    "  control mode  ",       // line2 (above value)
    "1: DUTY (defaul)",            // line4 * (below value)
    "2: CURRENT",              // line5 *
    "3: SPEED",            // line6
    "",              // line7
};

//###################################
//##### Traction Control System #####
//###################################
void tractionControlSystem_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    if (value == 1){
    objects->motorLeft->enableTractionControlSystem();
    objects->motorRight->enableTractionControlSystem();
    ESP_LOGW(TAG, "enabled Traction Control System");
    } else {
    objects->motorLeft->disableTractionControlSystem();
    objects->motorRight->disableTractionControlSystem();
    ESP_LOGW(TAG, "disabled Traction Control System");
    }
}
int tractionControlSystem_currentValue(display_task_parameters_t * objects)
{
    return (int)objects->motorLeft->getTractionControlSystemStatus();
}
menuItem_t item_tractionControlSystem = {
    tractionControlSystem_action,       // function action
    tractionControlSystem_currentValue, // function get initial value or NULL(show in line 2)
    NULL,                 // function get default value or NULL(dont set value, show msg)
    0,                    // valueMin
    1,                    // valueMax
    1,                    // valueIncrement
    "TCS / ASR       ",   // title
    "Traction Control",   // line1 (above value)
    "     System     ",   // line2 (above value)
    "1: enable       ",   // line4 * (below value)
    "0: disable      ",   // line5 *
    "note: requires  ",   // line6
    "speed ctl-mode  ",   // line7
};


//#####################
//####### RESET #######
//#####################
void item_reset_action(display_task_parameters_t *objects, SSD1306_t *display, int value)
{
    objects->buzzer->beep(1, 2000, 0);
    // close and erase NVS
    ESP_LOGW(TAG, "closing and ERASING non-volatile-storage...");
    nvs_close(*(objects->nvsHandle));
    ESP_ERROR_CHECK(nvs_flash_erase());
    // show message restarting
    ssd1306_clear_screen(display, false);
    displayTextLineCentered(display, 0, false, true, "");
    displayTextLineCentered(display, 1, true, true, "RE-");
    displayTextLineCentered(display, 4, true, true, "START");
    displayTextLineCentered(display, 7, false, true, "");
    vTaskDelay(1000 / portTICK_PERIOD_MS); // wait for buzzer to beep
    // restart
    ESP_LOGW(TAG, "RESTARTING");
    esp_restart();
}
menuItem_t item_reset = {
    item_reset_action,  // function action
    NULL,               // function get initial value or NULL(show in line 2)
    NULL,               // function get default value or NULL(dont set value, show msg)
    0,                  // valueMin
    0,                  // valueMax
    0,                  // valueIncrement
    "RESET defaults  ", // title
    "   reset nvs    ", // line1 (above value)
    "  and restart   ", // line2 <= showing "default = %d"
    "reset all stored", // line4 * (below value)
    "   parameters   ", // line5 *
    "",                 // line6
    "=>long to cancel", // line7
};


//###############################
//##### select statusScreen #####
//###############################
void item_statusScreen_action(display_task_parameters_t *objects, SSD1306_t *display, int value)
{
    switch (value)
    {
    case 1:
    default:
        display_selectStatusPage(STATUS_SCREEN_OVERVIEW);
        break;
    case 2:
        display_selectStatusPage(STATUS_SCREEN_SPEED);
        break;
    case 3:
        display_selectStatusPage(STATUS_SCREEN_JOYSTICK);
        break;
    case 4:
        display_selectStatusPage(STATUS_SCREEN_MOTORS);
        break;
    }
}
int item_statusScreen_value(display_task_parameters_t *objects)
{
    return 1; // initial value shown / changed from
}
menuItem_t item_statusScreen = {
    item_statusScreen_action, // function action
    item_statusScreen_value,  // function get initial value or NULL(show in line 2)
    NULL,                     // function get default value or NULL(dont set value, show msg)
    1,                        // valueMin
    4,                        // valueMax
    1,                        // valueIncrement
    "Status Screen   ",       // title
    "     Select     ",       // line1 (above value)
    "  Status Screen ",       // line2 (above value)
    "1: Overview",            // line4 * (below value)
    "2: Speeds",              // line5 *
    "3: Joystick",            // line6
    "4: Motors",              // line7
};

//#####################
//###### example ######
//#####################
void item_example_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    return;
}
int item_example_value(display_task_parameters_t * objects){
    return 53; //initial value shown / changed from
}
int item_example_valueDefault(display_task_parameters_t * objects){
    return 931; // optionally shown in line 2 as "default = %d"
}
menuItem_t item_example = {
    item_example_action, // function action
    item_example_value,  // function get initial value or NULL(show in line 2)
    NULL,                // function get default value or NULL(dont set value, show msg)
    -255,                // valueMin
    255,                 // valueMax
    2,                   // valueIncrement
    "example-item-max",  // title
    "line 1 - above  ",  // line1 (above value)
    "line 2 - above  ",  // line2 (above value)
    "line 4 - below  ",  // line4 * (below value)
    "line 5 - below  ",  // line5 *
    "line 6 - below  ",  // line6
    "line 7 - last   ",  // line7
};

menuItem_t item_last = {
    item_example_action, // function action
    item_example_value,  // function get initial value or NULL(show in line 2)
    item_example_valueDefault, // function get default value or NULL(dont set value, show msg)
    -500,                // valueMin
    4500,                // valueMax
    50,                  // valueIncrement
    "set large number",  // title
    "line 1 - above  ",  // line1 (above value)
    "line 2 - above  ",  // line2 (above value)
    "",                  // line4 * (below value)
    "",                  // line5 *
    "line 6 - below  ",  // line6
    "line 7 - last   ",  // line7
};


//####################################################
//### store all configured menu items in one array ###
//####################################################
const menuItem_t menuItems[] = {item_centerJoystick, item_calibrateJoystick, item_debugJoystick, item_statusScreen, item_maxDuty, item_maxRelativeBoost, item_accelLimit, item_decelLimit, item_brakeDecel, item_motorControlMode, item_tractionControlSystem, item_reset, item_example, item_last};
const int itemCount = 12;




//--------------------------
//------ showItemList ------
//--------------------------
//function that renders main menu to display (one update)
//list of all menu items with currently selected highlighted
#define SELECTED_ITEM_LINE 4
#define FIRST_ITEM_LINE 1
#define LAST_ITEM_LINE 7
void showItemList(SSD1306_t *display, int selectedItem)
{
    //-- show title line --
    displayTextLine(display, 0, false, true, "  --- menu ---  "); //inverted

    //-- show item list --
    for (int line = FIRST_ITEM_LINE; line <= LAST_ITEM_LINE; line++)
    { // loop through all lines
        int printItemIndex = selectedItem - SELECTED_ITEM_LINE + line;
        // TODO: when reaching end of items, instead of showing "empty" change position of selected item to still use the entire screen
        if (printItemIndex < 0 || printItemIndex >= itemCount) // out of range of available items
        {
            // no item in this line
            displayTextLineCentered(display, line, false, false, "---");
        }
        else
        {
            if (printItemIndex == selectedItem)
            {
                // selected item -> add '> ' and print inverted
                displayTextLine(display, line, false, true, "> %-14s", menuItems[printItemIndex].title); // inverted
            }
            else
            {
                // not the selected item -> print normal
                displayTextLine(display, line, false, false, "%-16s", menuItems[printItemIndex].title);
            }
        }
        // logging
        ESP_LOGD(TAG, "showItemList: loop - line=%d, item=%d, (selected=%d '%s')", line, printItemIndex, selectedItem, menuItems[selectedItem].title);
    }
}


//-----------------------------
//--- showValueSelectStatic ---
//-----------------------------
// function that renders lines that do not update of value-select screen to display (initial update)
// shows configured text of currently selected item
void showValueSelectStatic(display_task_parameters_t * objects, SSD1306_t *display, int selectedItem)
{
    //-- show title line --
    displayTextLine(display, 0, false, true, " -- set value -- "); // inverted

    //-- show text above value --
    displayTextLine(display, 1, false, false, "%-16s", menuItems[selectedItem].line1);

    //-- show line 2 or default value ---
    if (menuItems[selectedItem].defaultValue != NULL){
        displayTextLineCentered(display, 2, false, false, "default = %d", menuItems[selectedItem].defaultValue(objects));
    }
    else
    {
        // displayTextLine(display, 2, false, false, "previous=%d", menuItems[selectedItem].currentValue(objects)); // <= show previous value
        displayTextLine(display, 2, false, false, "%-16s", menuItems[selectedItem].line2);
    }

    //-- show value and other configured lines --
    // print value large, if two description lines are empty
    if (strlen(menuItems[selectedItem].line4) == 0 && strlen(menuItems[selectedItem].line5) == 0)
    {
        // print less lines: line5 and line6 only (due to large value)
        //displayTextLineCentered(display, 3, true, false, "%d", value); //large centered (value shown in separate function)
        displayTextLine(display, 6, false, false, "%-16s", menuItems[selectedItem].line6);
        displayTextLine(display, 7, false, false, "%-16s", menuItems[selectedItem].line7);
    }
    else
    {
        //displayTextLineCentered(display, 3, false, false, "%d", value); //centered (value shown in separate function)
        // print description lines 4 to 7
        displayTextLine(display, 4, false, false, "%-16s", menuItems[selectedItem].line4);
        displayTextLine(display, 5, false, false, "%-16s", menuItems[selectedItem].line5);
        displayTextLine(display, 6, false, false, "%-16s", menuItems[selectedItem].line6);
        displayTextLine(display, 7, false, false, "%-16s", menuItems[selectedItem].line7);
    }

    //-- show info msg instead of value --
    //when pointer to default value func not defined (set value not used, action only)
    if (menuItems[selectedItem].currentValue == NULL)
    {
        //show static text
        displayTextLineCentered(display, 3, false, true, "%s", "click to confirm");
    }
    // otherwise value gets updated in next iteration of menu-handle function
}


//-----------------------------
//----- updateValueSelect -----
//-----------------------------
// update line with currently set value only (increses performance significantly)
void updateValueSelect(SSD1306_t *display, int selectedItem)
{
    // print value large, if 2 description lines are empty
    if (strlen(menuItems[selectedItem].line4) == 0 && strlen(menuItems[selectedItem].line5) == 0)
    {
        // print large and centered value in line 3-5
        displayTextLineCentered(display, 3, true, false, "%d", value); // large centered
    }
    else
    {
        //print value centered in line 3
        displayTextLineCentered(display, 3, false, false, "%d", value); // centered
    }
}



//========================
//====== handleMenu ======
//========================
//controls menu with encoder input and displays the text on oled display
//function is repeatedly called by display task when in menu state
#define QUEUE_TIMEOUT 3000 //timeout no encoder event - to not block the display loop and actually handle menu-timeout
#define MENU_TIMEOUT 60000 //inactivity timeout (switch to IDLE mode) note: should be smaller than IDLE timeout in control task
void handleMenu(display_task_parameters_t * objects, SSD1306_t *display)
{
    static uint32_t lastActivity = 0;
    static int selectedItem = 0;
    rotary_encoder_event_t event; // store event data

    //--- handle different menu states ---
    switch (menuState)
    {
        //-------------------------
        //---- State MAIN MENU ----
        //-------------------------
    case MAIN_MENU:
        // update display
        showItemList(display, selectedItem); // shows list of items with currently selected one on display
        // wait for encoder event
        if (xQueueReceive(objects->encoderQueue, &event, QUEUE_TIMEOUT / portTICK_PERIOD_MS))
        {
            // reset menu- and control-timeout on any encoder event
            lastActivity = esp_log_timestamp();
            objects->control->resetTimeout(); // user input -> reset switch to IDLE timeout
            switch (event.type)
            {
            case RE_ET_CHANGED:
                //--- scroll in list ---
                if (event.diff < 0)
                {
                    if (selectedItem != itemCount - 1)
                    {
                        objects->buzzer->beep(1, 20, 0);
                        selectedItem++;
                        ESP_LOGD(TAG, "showing next item: %d '%s'", selectedItem, menuItems[selectedItem].title);
                    }
                    //note: display will update at start of next run
                }
                else
                {
                    if (selectedItem != 0)
                    {
                        objects->buzzer->beep(1, 20, 0);
                        selectedItem--;
                        ESP_LOGD(TAG, "showing previous item: %d '%s'", selectedItem, menuItems[selectedItem].title);
                    }
                    //note: display will update at start of next run
                }
                break;

            case RE_ET_BTN_CLICKED:
                //--- switch to edit value page ---
                objects->buzzer->beep(1, 50, 10);
                ESP_LOGI(TAG, "Button pressed - switching to state SET_VALUE");
                // change state (menu to set value)
                menuState = SET_VALUE;
                // clear display
                ssd1306_clear_screen(display, false);
                //update static content of set-value screen once at change only
                showValueSelectStatic(objects, display, selectedItem);
                // get currently configured value, when value-select feature is actually used in this item
                if (menuItems[selectedItem].currentValue != NULL)
                    value = menuItems[selectedItem].currentValue(objects);
                else
                    value = 0;
                break;

            case RE_ET_BTN_LONG_PRESSED:
                //--- exit menu mode ---
                // change to previous mode (e.g. JOYSTICK)
                objects->buzzer->beep(12, 15, 8);
                objects->control->toggleMode(controlMode_t::MENU); //currently already in MENU -> changes to previous mode
                ssd1306_clear_screen(display, false);
                break;

            case RE_ET_BTN_RELEASED:
            case RE_ET_BTN_PRESSED:
            break;
            }
        }
        break;

        //-------------------------
        //---- State SET VALUE ----
        //-------------------------
    case SET_VALUE:
        // update currently selected value
        // note: static lines are updated at mode change
        if (menuItems[selectedItem].currentValue != NULL) // dont update when set-value not used for this item
            updateValueSelect(display, selectedItem);

        // wait for encoder event
        if (xQueueReceive(objects->encoderQueue, &event, QUEUE_TIMEOUT / portTICK_PERIOD_MS))
        {
            objects->control->resetTimeout(); // user input -> reset switch to IDLE timeout
            switch (event.type)
            {
            case RE_ET_CHANGED:
                //-- change value --
                // no need to increment value when item configured to not show value
                if (menuItems[selectedItem].currentValue != NULL)
                {
                    objects->buzzer->beep(1, 25, 10);
                    // increment value
                    if (event.diff < 0)
                        value += menuItems[selectedItem].valueIncrement;
                    else
                        value -= menuItems[selectedItem].valueIncrement;
                    // limit to min/max range
                    if (value > menuItems[selectedItem].valueMax)
                        value = menuItems[selectedItem].valueMax;
                    if (value < menuItems[selectedItem].valueMin)
                        value = menuItems[selectedItem].valueMin;
                }
                break;
            case RE_ET_BTN_CLICKED:
                //-- apply value --
                ESP_LOGI(TAG, "Button pressed - running action function with value=%d for item '%s'", value, menuItems[selectedItem].title);
                objects->buzzer->beep(2, 50, 50);
                menuItems[selectedItem].action(objects, display, value);
                menuState = MAIN_MENU;
                break;
            case RE_ET_BTN_LONG_PRESSED:
                //-- exit value select to main menu --
                objects->buzzer->beep(2, 100, 50);
                ssd1306_clear_screen(display, false);
                menuState = MAIN_MENU;
                break;
            case RE_ET_BTN_PRESSED:
            case RE_ET_BTN_RELEASED:
                break;
            }
            // reset menu- and control-timeout on any encoder event
            lastActivity = esp_log_timestamp();
            objects->control->resetTimeout(); // user input -> reset switch to IDLE timeout
        }
        break;
    }


    //--------------------
    //--- menu timeout ---
    //--------------------
    //close menu and switch to IDLE mode when no encoder event occured within MENU_TIMEOUT
    if (esp_log_timestamp() - lastActivity > MENU_TIMEOUT)
    {
        ESP_LOGW(TAG, "TIMEOUT - no activity for more than %ds -> closing menu, switching to IDLE", MENU_TIMEOUT/1000);
        // reset menu
        selectedItem = 0;
        menuState = MAIN_MENU;
        ssd1306_clear_screen(display, false);
        // change control mode
        objects->control->changeMode(controlMode_t::IDLE);
        return;
    }
}
