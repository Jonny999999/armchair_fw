#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "driver/ledc.h"
#include "esp_err.h"
}

#include <cmath>


//====================================
//===== single100a motor driver ======
//====================================

//--------------------------------------------
//---- struct, enum, variable declarations ---
//--------------------------------------------
//motorstate_t, motorstateStr outsourced to common/types.hpp
#include "types.hpp"

//struct with all config parameters for single100a motor driver
typedef struct single100a_config_t {
    gpio_num_t gpio_pwm;
    gpio_num_t gpio_a;
    gpio_num_t gpio_b;
    ledc_timer_t ledc_timer;
    ledc_channel_t ledc_channel;
	bool aEnabledPinState;
	bool bEnabledPinState;
    ledc_timer_bit_t resolution;
    int pwmFreq;
} single100a_config_t;



//--------------------------------
//------- single100a class -------
//--------------------------------
class single100a {
    public:
        //--- constructor ---
        single100a(single100a_config_t config_f); //provide config struct (see above)

        //--- functions ---
        void set(motorstate_t state, float duty_f = 0); //set mode and duty of the motor (see motorstate_t above)
        //TODO: add functions to get the current state and duty

    private:
        //--- functions ---
        void init(); //initialize pwm and gpio outputs, calculate maxDuty

        //--- variables ---
        single100a_config_t config;
        uint32_t dutyMax;
        motorstate_t state = motorstate_t::IDLE;
};
