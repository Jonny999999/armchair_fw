#pragma once

extern "C"
{
#include "driver/gpio.h"
}

#include "motorctl.hpp"


//--- fan_config_t ---
//struct with all config parameters for a fan
typedef struct fan_config_t {
    gpio_num_t gpio_fan;
    float dutyThreshold;
	uint32_t minOnMs;
	uint32_t minOffMs;
	uint32_t turnOffDelayMs;
} fan_config_t;


// struct with variables passed to task from main
typedef struct task_fans_parameters_t {
    fan_config_t fan_config;
    controlledMotor * motorLeft;
    controlledMotor * motorRight;
} task_fans_parameters_t;


//====================================
//========== motorctl task ===========
//====================================
//note: pointer to task_fans_parameters_t has to be passed as task-parameter (config, motor objects)
void task_fans( void * task_fans_parameters );



//==================================
//====== controlledFan class =======
//==================================
class controlledFan {
    public:
        //--- constructor ---
        controlledFan (fan_config_t config_f, controlledMotor* motor1_f, controlledMotor* motor2_f );

        //--- functions ---
        void handle(); //has to be run repeatedly in a slow loop


    private:
        //--- variables ---
		bool fanRunning = false;
		bool needsCooling = false;
		uint32_t timestamp_needsCoolingSet;
        uint32_t timestamp_lastThreshold = 0;
		uint32_t timestamp_turnedOn = 0;
		uint32_t timestamp_turnedOff = 0;
        fan_config_t config;
        controlledMotor * motor1;
        controlledMotor * motor2;

        motorCommand_t motor1Status;
        motorCommand_t motor2Status;
};
