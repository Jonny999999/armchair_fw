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
//enum for special instructions / commands to be run in control task
enum class auto_instruction_t { NONE, SWITCH_PREV_MODE, SWITCH_JOYSTICK_MODE, RESET_ACCEL_DECEL, RESET_ACCEL, RESET_DECEL };

//struct for a simple command
//e.g. put motors in a certain state for certain time
typedef struct commandSimple_t{
    motorCommands_t motorCmds;
    uint32_t msDuration;
    uint32_t fadeDecel;
    uint32_t fadeAccel;
    auto_instruction_t instruction;
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
        //TODO: go with other approach: separate task for handling auto mode
        //  - receive commands with queue anyways
        //  - => use delay function
        //  - have a queue that outputs current motor state/commands -> repeatedly check the queue in control task
        //function that handles automatic driving and returns motor commands
        //also provides instructions to be executed in control task via pointer
        motorCommands_t generateCommands(auto_instruction_t * instruction);

        //function that adds a basic command to the queue
        void addCommand(commandSimple_t command);
        //function that adds an array of basic commands to queue
        void addCommands(commandSimple_t commands[], size_t count);

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
        const motorCommand_t motorCmd_motorIdle = {
            .state = motorstate_t::IDLE,
            .duty = 0
        };
        const motorCommands_t motorCmds_bothMotorsIdle = {
            .left = motorCmd_motorIdle,
            .right = motorCmd_motorIdle
        };
};


