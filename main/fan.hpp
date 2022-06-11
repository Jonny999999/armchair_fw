#pragma once

extern "C"
{
#include "driver/gpio.h"
}

#include "motorctl.hpp"



//struct with all config parameters for a fan
typedef struct fan_config_t {
    gpio_num_t gpio_fan;
    uint32_t msRun;
    float dutyThreshold;
} fan_config;



//==================================
//====== controlledFan class =======
//==================================
class controlledFan {
    public:
        //--- constructor ---
        controlledFan (fan_config_t config_f, controlledMotor* motor_f );

        //--- functions ---
        void handle();


    private:
        //--- variables ---
        uint32_t timestamp_lastThreshold;
        fan_config_t config;
        controlledMotor * motor;

        motorCommand_t motorStatus;
};
