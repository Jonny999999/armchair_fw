#pragma once

#include "display.hpp"


//--- menuState_t ---
// modes the menu can be in
typedef enum {
    MAIN_MENU = 0,
    SET_VALUE
} menuState_t;


//--- menuItem_t ---
// struct describes one menu element (all defined in menu.cpp)
typedef struct
{
    void (*action)(display_task_parameters_t * objects, SSD1306_t * display, int value);   // pointer to function run when confirmed
    int (*currentValue)(display_task_parameters_t * objects); // pointer to function to get currently configured value
    int valueMin;          // min allowed value
    int valueMax;          // max allowed value
    int valueIncrement;    // amount changed at one encoder tick (+/-)
    const char title[17];  // shown in list
    const char line1[17];  // above value
    const char line2[17];  // above value
    const char line4[17];  // below value *
    const char line5[17];  // below value *
    const char line6[17];  // below value
    const char line7[17];  // below value
} menuItem_t;

void handleMenu(display_task_parameters_t * objects, SSD1306_t *display);