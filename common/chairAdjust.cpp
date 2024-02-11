extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
}

#include "chairAdjust.hpp"



//--- gloabl variables ---
// strings for logging the rest state
const char* restStateStr[] = {"REST_OFF", "REST_DOWN", "REST_UP"};

//--- local variables ---
//tag for logging
static const char * TAG = "chair-adjustment";



//=============================
//======== constructor ========
//=============================
cControlledRest::cControlledRest(gpio_num_t gpio_up_f, gpio_num_t gpio_down_f, const char * name_f){
    strcpy(name, name_f);
    gpio_up = gpio_up_f;
    gpio_down = gpio_down_f;
    init();
}



//====================
//======= init =======
//====================
// init gpio pins for relays
void cControlledRest::init()
{
    ESP_LOGW(TAG, "[%s] initializing gpio pins %d, %d for relays...", name, gpio_up, gpio_down);
    // configure 2 gpio pins
    gpio_pad_select_gpio(gpio_up);
    gpio_set_direction(gpio_up, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(gpio_down);
    gpio_set_direction(gpio_down, GPIO_MODE_OUTPUT);
    // both relays off initially
    gpio_set_level(gpio_down, 0);
    gpio_set_level(gpio_up, 0);
}



//============================
//========= setState =========
//============================
void cControlledRest::setState(restState_t targetState)
{
    //check if actually changed
    if (targetState == state){
        ESP_LOGD(TAG, "[%s] state already at '%s', nothing to do", name, restStateStr[state]);
        return;
    }

    //apply new state
    ESP_LOGI(TAG, "[%s] switching from state '%s' to '%s'", name, restStateStr[state], restStateStr[targetState]);
    state = targetState;
    timestamp_lastChange = esp_log_timestamp(); //TODO use this to estimate position
    switch (state)
    {
    case REST_UP:
        gpio_set_level(gpio_down, 0);
        gpio_set_level(gpio_up, 1);
        break;
    case REST_DOWN:
        gpio_set_level(gpio_down, 1);
        gpio_set_level(gpio_up, 0);
        break;
    case REST_OFF:
        gpio_set_level(gpio_down, 1);
        gpio_set_level(gpio_up, 0);
        break;
    }
}



//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
//TODO:
// - add separate task that controls chair adjustment
//    - timeout
//    - track position
//    - auto-adjust: move to position while driving
//    - control via app
// - add delay betweem direction change
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest){
	//--- variables ---
    float stickThreshold = 0.3; //min coordinate for motor to start

    //--- control rest motors ---
    //leg rest (x-axis)
    if (data.x > stickThreshold) legRest->setState(REST_UP);
    else if (data.x < -stickThreshold) legRest->setState(REST_DOWN);
    //back rest (y-axis)
    if (data.y > stickThreshold) backRest->setState(REST_UP);
    else if (data.y < -stickThreshold) backRest->setState(REST_DOWN);
}
