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
void item_centerJoystick_action(int value){
    if (!value) return;
    ESP_LOGW(TAG, "defining joystick center");
    joystick.defineCenter();
}
int item_centerJoystick_value(){
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
//##### set max duty #####
//########################
void maxDuty_action(int value)
{
    //TODO actually store the value
    ESP_LOGW(TAG, "set max duty to %d", value);
}
int maxDuty_currentValue()
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
void item_accelLimit_action(int value){
    motorLeft.setFade(fadeType_t::ACCEL, (uint32_t)value);
    motorRight.setFade(fadeType_t::ACCEL, (uint32_t)value);
}
int item_accelLimit_value()
{
    return motorLeft.getFade(fadeType_t::ACCEL);
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
void item_decelLimit_action(int value)
{
    motorLeft.setFade(fadeType_t::DECEL, (uint32_t)value);
    motorRight.setFade(fadeType_t::DECEL, (uint32_t)value);
}
int item_decelLimit_value()
{
    return motorLeft.getFade(fadeType_t::DECEL);
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
void item_example_action(int value){
    return;
}
int item_example_value(){
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
menuItem_t menuItems[] = {item_centerJoystick, item_maxDuty, item_accelLimit, item_decelLimit, item_example, item_last};
int itemCount = 6;




//--------------------------
//------ showItemList ------
//--------------------------
#define SELECTED_ITEM_LINE 4
#define FIRST_ITEM_LINE 1
#define LAST_ITEM_LINE 7
void showItemList(SSD1306_t *display,  int selectedItem)
{
    //--- variables ---
    char buf[20];
    int len;

    //-- show title line --
    len = snprintf(buf, 19, "  --- menu ---  ");
    ssd1306_display_text(display, 0, buf, len, true);

    //-- show item list --
    for (int line = FIRST_ITEM_LINE; line <= LAST_ITEM_LINE; line++)
    { // loop through all lines
        int printItemIndex = selectedItem - SELECTED_ITEM_LINE + line;
        // TODO: when reaching end of items, instead of showing "empty" change position of selected item to still use the entire screen
        if (printItemIndex < 0 || printItemIndex >= itemCount) // out of range of available items
        {
            len = snprintf(buf, 20, "  -- empty --   ");
        }
        else
        {
            if (printItemIndex == selectedItem)
            {
                // add '> <' when selected item
                len = snprintf(buf, 20, "> %-13s<", menuItems[printItemIndex].title);
            }
            else
            {
                len = snprintf(buf, 20, "%-16s", menuItems[printItemIndex].title);
            }
        }
        // update display line
        ssd1306_display_text(display, line, buf, len, line == SELECTED_ITEM_LINE);
        // logging
        ESP_LOGD(TAG, "showItemList: loop - line=%d, item=%d, (selected=%d '%s')", line, printItemIndex, selectedItem, menuItems[selectedItem].title);
    }
}




//---------------------------
//----- showValueSelect -----
//---------------------------
// TODO show previous value in one line?
// TODO update changed line only (value)
void showValueSelect(SSD1306_t *display, int selectedItem)
{
    //--- variables ---
    char buf[20];
    int len;

    //-- show title line --
    len = snprintf(buf, 19, " -- set value -- ");
    ssd1306_display_text(display, 0, buf, len, true);

    //-- show text above value --
    len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line1);
    ssd1306_display_text(display, 1, buf, len, false);
    len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line2);
    ssd1306_display_text(display, 2, buf, len, false);

    //-- show value and other configured lines --
    // print value large, if 2 description lines are empty
    if (strlen(menuItems[selectedItem].line4) == 0 && strlen(menuItems[selectedItem].line5) == 0 )
    {
        //print large value + line5 and line6
        len = snprintf(buf, 20, " %d   ", value);
        ssd1306_display_text_x3(display, 3, buf, len, false);
        len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line6);
        ssd1306_display_text(display, 6, buf, len, false);
        len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line7);
        ssd1306_display_text(display, 7, buf, len, false);
    }
    else
    {
        // print value centered
        int numDigits = snprintf(NULL, 0, "%d", value);
        int numSpaces = (16 - numDigits) / 2;
        snprintf(buf, sizeof(buf), "%*s%d%*s", numSpaces, "", value, 16 - numSpaces - numDigits, "");
        ESP_LOGD(TAG, "showValueSelect: center number - value=%d, needed-spaces=%d, resulted-string='%s'", value, numSpaces, buf);
        ssd1306_display_text(display, 3, buf, len, false);
        // print description lines 4 to 7
        len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line4);
        ssd1306_display_text(display, 4, buf, len, false);
        len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line5);
        ssd1306_display_text(display, 5, buf, len, false);
        len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line6);
        ssd1306_display_text(display, 6, buf, len, false);
        len = snprintf(buf, 20, "%-16s", menuItems[selectedItem].line7);
        ssd1306_display_text(display, 7, buf, len, false);
    }
}




//========================
//====== handleMenu ======
//========================
//controls menu with encoder input and displays the text on oled display
//function is repeatedly called when in menu state
#define QUEUE_TIMEOUT 3000 //timeout no encoder event - to handle timeout and not block the display loop
#define MENU_TIMEOUT 30000 //inactivity timeout (switch to IDLE mode)
void handleMenu(SSD1306_t *display)
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
        if (xQueueReceive(encoderQueue, &event, QUEUE_TIMEOUT / portTICK_PERIOD_MS))
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
                value = menuItems[selectedItem].currentValue();
                // clear display
                ssd1306_clear_screen(display, false);
                break;

            //exit menu mode
            case RE_ET_BTN_LONG_PRESSED:
                //change to previous mode (e.g. JOYSTICK)
                control.toggleMode(controlMode_t::MENU); //currently already in MENU -> changes to previous mode
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

        if (xQueueReceive(encoderQueue, &event, QUEUE_TIMEOUT / portTICK_PERIOD_MS))
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
                menuItems[selectedItem].action(value);
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
        control.changeMode(controlMode_t::IDLE);
        return;
    }
}