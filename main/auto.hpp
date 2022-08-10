#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
}

#include "freertos/queue.h"
#include <cmath>
#include "motorctl.hpp"



//--------------------------------------------
//---- struct, enum, variable declarations ---
//--------------------------------------------
//struct for a simple command
//e.g. put motors in a certain state for certain time
typedef struct {
    motorCommands_t commands;
    uint32_t msDuration;
} commandSimple_t;



//------------------------------------
//----- automatedArmchair class  -----
//------------------------------------
class automatedArmchair {
    public:
        //--- methods ---
        //constructor
        automatedArmchair(void);
        //function to generate motor commands
        //can be also seen as handle function 
        //TODO: maybe create separate task for handling at mode switch and communicate with queue?
        motorCommands_t generateCommands(); //repeatedly called by control task

        //function that adds a basic command to the queue
        void addCommand(commandSimple_t command);

        //function that deletes all pending/queued commands
        motorCommands_t clearCommands();


    private:
        //--- methods ---
        //--- objects ---
        //TODO: add buzzer here
        //--- variables ---
        //queue for storing pending commands
        QueueHandle_t commandQueue = NULL;
        //current running command
        commandSimple_t cmdCurrent;
        //timestamp current command is finished
        uint32_t timestampCmdFinished = 0;

        motorCommands_t motorCommands;

        //command preset for idling motors
        const motorCommand_t cmd_motorIdle = {
            .state = motorstate_t::IDLE,
            .duty = 0
        };
        const motorCommands_t cmds_bothMotorsIdle = {
            .left = cmd_motorIdle,
            .right = cmd_motorIdle
        };
};


