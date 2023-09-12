extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ssd1306.h"
#include "font8x8_basic.h"
}

#include "config.hpp"


//task that inititialized the display, displays welcome message 
//and releatedly updates the display with certain content
void display_task( void * pvParameters );
