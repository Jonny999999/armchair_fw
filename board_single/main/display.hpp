#pragma once

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ssd1306.h"
#include "font8x8_basic.h"
}


#include "joystick.hpp"
#include "control.hpp"
#include "speedsensor.hpp"

// configuration for initializing display (passed to task as well)
typedef struct display_config_t {
    // initialization
    gpio_num_t gpio_scl;
    gpio_num_t gpio_sda;
    int gpio_reset; // negative number means reset pin is not connected or not used
    int width;
    int height;
    int offsetX;
    bool flip;
    // display-task
    int contrastNormal;
    int contrastReduced;
    uint32_t timeoutReduceContrastMs;
    uint32_t timeoutSwitchToScreensaverMs;
} display_config_t;


// struct with variables passed to task from main()
typedef struct display_task_parameters_t {
    display_config_t displayConfig;
    controlledArmchair * control;
    evaluatedJoystick * joystick;
    QueueHandle_t encoderQueue;
    controlledMotor * motorLeft;
    controlledMotor * motorRight;
    speedSensor * speedLeft;
    speedSensor * speedRight;
    buzzer_t *buzzer;
    nvs_handle_t * nvsHandle;
} display_task_parameters_t;


// enum for selecting the currently shown status page (display content when not in MENU_SETTINGS mode)
typedef enum displayStatusPage_t {STATUS_SCREEN_OVERVIEW=0, STATUS_SCREEN_SPEED, STATUS_SCREEN_JOYSTICK, STATUS_SCREEN_MOTORS, STATUS_SCREEN_SCREENSAVER, __NUMBER_OF_AVAILABLE_SCREENS} displayStatusPage_t; //note: SCREENSAVER has to be last one since it is ignored by rotate and used to determine count

// get precise battery voltage (using lookup table)
float getBatteryVoltage();

// get battery charge level in percent (using lookup table as discharge curve)
float getBatteryPercent();

// function to select one of the defined status screens which are shown on display when not in MENU_SETTINGS or MENU_SELECT_MODE mode
void display_selectStatusPage(displayStatusPage_t newStatusPage);
// select next/previous status screen to be shown, when noRotate is set is stays at first/last screen
void display_rotateStatusPage(bool reverseDirection = false, bool noRotate = false);

// show content for certain time in any mode
void display_showNotification(uint32_t showDurationMs, const char *line1, const char * line2Large, const char * line3Large );

//task that inititialized the display, displays welcome message 
//and releatedly updates the display with certain content
void display_task( void * pvParameters );

//abstracted function for printing one line on the display, using a format string directly
//and options: Large-font (3 lines, max 5 digits), or inverted color
void displayTextLine(SSD1306_t *display, int line, bool large, bool inverted, const char *format, ...);

//abstracted function for printing a string CENTERED on the display, using a format string
//adds spaces left and right to fill the line (if not too long already)
void displayTextLineCentered(SSD1306_t *display, int line, bool isLarge, bool inverted, const char *format, ...);