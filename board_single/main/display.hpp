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
    gpio_num_t gpio_scl;
    gpio_num_t gpio_sda;
    gpio_num_t gpio_reset;
    int width;
    int height;
    int offsetX;
    bool flip;
    int contrast;
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


//task that inititialized the display, displays welcome message 
//and releatedly updates the display with certain content
void display_task( void * pvParameters );

//abstracted function for printing one line on the display, using a format string directly
//and options: Large-font (3 lines, max 5 digits), or inverted color
void displayTextLine(SSD1306_t *display, int line, bool large, bool inverted, const char *format, ...);

//abstracted function for printing a string CENTERED on the display, using a format string
//adds spaces left and right to fill the line (if not too long already)
void displayTextLineCentered(SSD1306_t *display, int line, bool isLarge, bool inverted, const char *format, ...);