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
#include "config.hpp"
#include "motorctl.hpp"


//--- variables ---
static const char *TAG = "menu";
static menuState_t menuState = MAIN_MENU;
static int value = 0;



//================================
//===== CONFIGURE MENU ITEMS =====
//================================
// note: when line4 * and line5 * are empty the value is printed large

//#########################
//#### center Joystick ####
//#########################
void item_centerJoystick_action(display_task_parameters_t * objects, SSD1306_t * display, int value){
    if (!value) return;
    ESP_LOGW(TAG, "defining joystick center");
    (*objects).joystick->defineCenter();
    //objects->joystick->defineCenter();
    //joystick->defineCenter();
}
int item_centerJoystick_value(display_task_parameters_t * objects){
    return 1;
}

menuItem_t item_centerJoystick = {
    item_centerJoystick_action, // function action
    item_centerJoystick_value,
    0,                      // valueMin
    1,                      // valueMAx
    1,                      // valueIncrement
    "Center Joystick",      // title
    "Center Joystick", // line1 (above value)
    "click to confirm",   // line2 (above value)
    "defines current",                     // line4 * (below value)
    "pos as center",                     // line5 *
    "click to confirm",     // line6
    "set 0 to cancel",  // line7
};


//########################
//#### debug Joystick ####
//########################
//continously show/update joystick data on display
void item_debugJoystick_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    //--- variables ---
    bool running = true;
    rotary_encoder_event_t event;

    //-- pre loop instructions --
    if (!value) // dont open menu when value was set to 0
        return;
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
        if (xQueueReceive(objects->encoderQueue, &event, 20 / portTICK_PERIOD_MS))
        {
            switch (event.type)
            {
            case RE_ET_BTN_CLICKED:
                running = false;
                break;
            case RE_ET_CHANGED:
            case RE_ET_BTN_PRESSED:
            case RE_ET_BTN_RELEASED:
            case RE_ET_BTN_LONG_PRESSED:
                break;
            }
        }
    }
}

int item_debugJoystick_value(display_task_parameters_t * objects){
    return 1;
}

menuItem_t item_debugJoystick = {
    item_debugJoystick_action, // function action
    item_debugJoystick_value,
    0,                      // valueMin
    1,                      // valueMAx
    1,                      // valueIncrement
    "Debug joystick",      // title
    "Debug joystick", // line1 (above value)
    "",   // line2 (above value)
    "click to enter",                     // line4 * (below value)
    "debug screen",                     // line5 *
    "prints values",     // line6
    "set 0 to cancel",  // line7
};


//########################
//##### set max duty #####
//########################
void maxDuty_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    //TODO actually store the value
    ESP_LOGW(TAG, "set max duty to %d", value);
}
int maxDuty_currentValue(display_task_parameters_t * objects)
{
    //TODO get real current value
    return 84;
}
menuItem_t item_maxDuty = {
    maxDuty_action, // function action
    maxDuty_currentValue,
    1,                  // valueMin
    99,                 // valueMAx
    1,                  // valueIncrement
    "max duty",     // title
    "",                 // line1 (above value)
    "  set max-duty: ", // line2 (above value)
    "",                 // line4 * (below value)
    "",                 // line5 *
    "      1-99      ", // line6
    "     percent    ", // line7
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
menuItem_t item_accelLimit = {
    item_accelLimit_action, // function action
    item_accelLimit_value,
    0,                // valueMin
    10000,            // valueMAx
    100,              // valueIncrement
    "Accel limit",    // title
    "Accel limit /",  // line1 (above value)
    "Fade up time",   // line2 (above value)
    "",               // line4 * (below value)
    "",               // line5 *
    "milliseconds",   // line6
    "from 0 to 100%", // line7
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
menuItem_t item_decelLimit = {
    item_decelLimit_action, // function action
    item_decelLimit_value,
    0,                // valueMin
    10000,            // valueMAx
    100,              // valueIncrement
    "Decel limit",    // title
    "Decel limit /",  // line1 (above value)
    "Fade down time", // line2 (above value)
    "",               // line4 * (below value)
    "",               // line5 *
    "milliseconds",   // line6
    "from 100 to 0%", // line7
};

//#####################
//###### example ######
//#####################
void item_example_action(display_task_parameters_t * objects, SSD1306_t * display, int value)
{
    return;
}
int item_example_value(display_task_parameters_t * objects){
    return 53;
}
menuItem_t item_example = {
    item_example_action, // function action
    item_example_value,
    -255,             // valueMin
    255,              // valueMAx
    2,                // valueIncrement
    "example-item-max",     // title
    "line 1 - above", // line1 (above value)
    "line 2 - above", // line2 (above value)
    "line 4 - below", // line4 * (below value)
    "line 5 - below", // line5 *
    "line 6 - below", // line6
    "line 7 - last",  // line7
};

menuItem_t item_last = {
    item_example_action, // function action
    item_example_value,
    -500,               // valueMin
    4500,               // valueMAx
    50,                 // valueIncrement
    "set large number", // title
    "line 1 - above",   // line1 (above value)
    "line 2 - above",   // line2 (above value)
    "",                 // line4 * (below value)
    "",                 // line5 *
    "line 6 - below",   // line6
    "line 7 - last",    // line7

};

//store all configured menu items in one array
menuItem_t menuItems[] = {item_centerJoystick, item_debugJoystick, item_accelLimit, item_decelLimit, item_example, item_last};
int itemCount = 6;




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
            displayTextLine(display, line, false, false, "  -- empty --   ");
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

//---------------------------
//----- showValueSelect -----
//---------------------------
// function that renders value-select screen to display (one update)
// shows configured text of selected item and currently selected value
// TODO show previous value in one line?
// TODO update changed line only (value)
void showValueSelect(SSD1306_t *display, int selectedItem)
{
    //-- show title line --
    displayTextLine(display, 0, false, true, " -- set value -- "); // inverted

    //-- show text above value --
    displayTextLine(display, 1, false, false, "%-16s", menuItems[selectedItem].line1);
    displayTextLine(display, 2, false, false, "%-16s", menuItems[selectedItem].line2);

    //-- show value and other configured lines --
    // print value large, if 2 description lines are empty
    if (strlen(menuItems[selectedItem].line4) == 0 && strlen(menuItems[selectedItem].line5) == 0)
    {
        // print large value + line5 and line6
        displayTextLineCentered(display, 3, true, false, "%d", value); //large centered
        displayTextLine(display, 6, false, false, "%-16s", menuItems[selectedItem].line6);
        displayTextLine(display, 7, false, false, "%-16s", menuItems[selectedItem].line7);
    }
    else
    {
        displayTextLineCentered(display, 3, false, false, "%d", value); //centered
        // print description lines 4 to 7
        displayTextLine(display, 4, false, false, "%-16s", menuItems[selectedItem].line4);
        displayTextLine(display, 5, false, false, "%-16s", menuItems[selectedItem].line5);
        displayTextLine(display, 6, false, false, "%-16s", menuItems[selectedItem].line6);
        displayTextLine(display, 7, false, false, "%-16s", menuItems[selectedItem].line7);
    }
}




//========================
//====== handleMenu ======
//========================
//controls menu with encoder input and displays the text on oled display
//function is repeatedly called by display task when in menu state
#define QUEUE_TIMEOUT 3000 //timeout no encoder event - to handle timeout and not block the display loop
#define MENU_TIMEOUT 60000 //inactivity timeout (switch to IDLE mode)
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
            lastActivity = esp_log_timestamp();
            switch (event.type)
            {
            case RE_ET_CHANGED:
                //--- scroll in list ---
                if (event.diff < 0)
                {
                    if (selectedItem != itemCount - 1)
                        selectedItem++;
                    ESP_LOGD(TAG, "showing next item: %d '%s'", selectedItem, menuItems[selectedItem].title);
                    //note: display will update at start of next run
                }
                else
                {
                    if (selectedItem != 0)
                        selectedItem--;
                    ESP_LOGD(TAG, "showing previous item: %d '%s'", selectedItem, menuItems[selectedItem].title);
                    //note: display will update at start of next run
                }
                break;

            case RE_ET_BTN_CLICKED:
                //--- switch to edit value page ---
                ESP_LOGI(TAG, "Button pressed - switching to state SET_VALUE");
                // change state (menu to set value)
                menuState = SET_VALUE;
                // get currently configured value
                value = menuItems[selectedItem].currentValue(objects);
                // clear display
                ssd1306_clear_screen(display, false);
                break;

            //exit menu mode
            case RE_ET_BTN_LONG_PRESSED:
                //change to previous mode (e.g. JOYSTICK)
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
        // wait for encoder event
        showValueSelect(display, selectedItem);

        if (xQueueReceive(objects->encoderQueue, &event, QUEUE_TIMEOUT / portTICK_PERIOD_MS))
        {
            lastActivity = esp_log_timestamp();
            switch (event.type)
            {
            case RE_ET_CHANGED:
                //-- change value --
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
                break;
            case RE_ET_BTN_CLICKED:
                //-- apply value --
                ESP_LOGI(TAG, "Button pressed - running action function with value=%d for item '%s'", value, menuItems[selectedItem].title);
                menuItems[selectedItem].action(objects, display, value);
                menuState = MAIN_MENU;
                break;
            case RE_ET_BTN_PRESSED:
            case RE_ET_BTN_RELEASED:
            case RE_ET_BTN_LONG_PRESSED:
                break;
            }
        }
        break;
    }


    //--------------------
    //--- menu timeout ---
    //--------------------
    //close menu and switch to IDLE mode when no encoder event within MENU_TIMEOUT
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