#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
}

#include "motordrivers.hpp"


//-------------------------------------
//-------- struct  declarations -------
//-------------------------------------

//struct for sending command for one motor in the queue
struct motorCommand_t {
    motorstate_t state;
    float duty;
};

//struct containing commands for two motors
typedef struct motorCommands_t {
    motorCommand_t left;
    motorCommand_t right;
} motorCommands_t;

//struct with all config parameters for a motor regarding ramp and current limit
typedef struct motorctl_config_t {
    uint32_t msFade; //acceleration of the motor (ms it should take from 0 to 100%)
    float currentMax;
} motorctl_config_t;



class controlledMotor {
    public:
        //--- functions ---
        controlledMotor(single100a_config_t config_driver,  motorctl_config_t config_control); //constructor with structs for configuring motordriver and parameters for control TODO: add configuration for currentsensor
        void handle(); //controls motor duty with fade and current limiting feature (has to be run frequently by another task)
        void setTarget(motorstate_t state_f, float duty_f = 0); //adds target command to queue for handle function


    private:
        //--- functions ---
        void init(); //creates currentsensor objects, motordriver objects and queue

        //--- objects ---
        //TODO: add currentsensor object
        //motor driver
        single100a motor;
        //queue for sending commands to the separate task running the handle() function very fast
        QueueHandle_t commandQueue = NULL;

        //--- variables ---
        //struct for storing control specific parameters
        motorctl_config_t config;

        motorstate_t state = motorstate_t::IDLE;

        float currentMax;
        float currentNow;

        float dutyTarget;
        float dutyNow;
        float dutyIncrement;
        float dutyDelta;

        uint32_t ramp;
        int64_t timestampLastRunUs;

        struct motorCommand_t commandReceive = {};
        struct motorCommand_t commandSend = {};



};
