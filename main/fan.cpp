extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

#include "fan.hpp"


//tag for logging
static const char * TAG = "fan-control";


//-----------------------------
//-------- constructor --------
//-----------------------------
controlledFan::controlledFan(fan_config_t config_f, controlledMotor* motor_f ){
    //copy config
    config = config_f;
    //copy pointer to motor object
    motor = motor_f;

    //initialize gpio pin
    gpio_pad_select_gpio(config.gpio_fan);
    gpio_set_direction(config.gpio_fan, GPIO_MODE_OUTPUT);
}



//--------------------------
//--------- handle ---------
//--------------------------
void controlledFan::handle(){
    //get current state of the motor
    motorStatus = motor->getStatus();

    //TODO Add statemachine for more specific control? Exponential average?
    //update timestamp if threshold exceeded
    if (motorStatus.duty > config.dutyThreshold){
        timestamp_lastThreshold = esp_log_timestamp();
    }

    //turn fan on if time since last exceeded threshold is less than msRun
    if (esp_log_timestamp() - timestamp_lastThreshold < config.msRun) {
        gpio_set_level(config.gpio_fan, 1);        
        ESP_LOGD(TAG, "fan is on (gpio: %d)", (int)config.gpio_fan);
    }
    //otherwise turn fan off
    else {
        gpio_set_level(config.gpio_fan, 0);        
        ESP_LOGD(TAG, "fan is off (gpio: %d)", (int)config.gpio_fan);
    }
}



