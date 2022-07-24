#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "buzzer.hpp"
#include "http.hpp"


//--------------------------------------------
//---- struct, enum, variable declarations ---
//--------------------------------------------
//enum that decides how the motors get controlled
enum class controlMode_t {IDLE, JOYSTICK, MASSAGE, HTTP, MQTT, BLUETOOTH, AUTO};
//string array representing the mode enum (for printing the state as string)
extern const char* controlModeStr[7];

//struct with config parameters
typedef struct control_config_t {
    controlMode_t defaultMode;  //default mode after startup and toggling IDLE
    //--- timeout ---
    uint32_t timeoutMs;         //time of inactivity after which the mode gets switched to IDLE
    float timeoutTolerancePer;  //percentage the duty can vary between timeout checks considered still inactive
} control_config_t;




//==================================
//========= control class ==========
//==================================
//controls the mode the armchair operates
//repeatedly generates the motor commands corresponding to current mode and sends those to motorcontrol
class controlledArmchair {
    public:
        //--- constructor ---
        controlledArmchair (
                control_config_t config_f,
                buzzer_t* buzzer_f,
                controlledMotor* motorLeft_f,
                controlledMotor* motorRight_f,
                evaluatedJoystick* joystick_f,
                httpJoystick* httpJoystick_f
                );

        //--- functions ---
        //task that repeatedly generates motor commands depending on the current mode
        void startHandleLoop();

        //function that changes to a specified control mode
        void changeMode(controlMode_t modeNew);

        //function that toggle between IDLE and previous active mode (or default if not switched to certain mode yet)
        void toggleIdle();

        //function that toggles between two modes, but prefers first argument if entirely different mode is currently active
        void toggleModes(controlMode_t modePrimary, controlMode_t modeSecondary);

        //function that restarts timer which initiates the automatic timeout (switch to IDLE) after certain time of inactivity
        void resetTimeout();

    private:

        //--- functions ---
        //function that evaluates whether there is no activity/change on the motor duty for a certain time, if so a switch to IDLE is issued. - has to be run repeatedly in a slow interval
        void handleTimeout();

        //--- objects ---
        buzzer_t* buzzer;
        controlledMotor* motorLeft;
        controlledMotor* motorRight;
        httpJoystick* httpJoystickMain_l;
        evaluatedJoystick* joystick_l;

        //---variables ---
        //struct for motor commands returned by generate functions of each mode
        motorCommands_t commands;
        //struct with config parameters
        control_config_t config;

        //store joystick data
        joystickData_t stickData;

        //variables for http mode
        uint32_t http_timestamp_lastData = 0;

        //definition of mode enum
        controlMode_t mode = controlMode_t::IDLE;

        //variable to store mode when toggling IDLE mode 
        controlMode_t modePrevious; //default mode

        //command preset for idling motors
        const motorCommand_t cmd_motorIdle = {
            .state = motorstate_t::IDLE,
            .duty = 0
        };
        const motorCommands_t cmds_bothMotorsIdle = {
            .left = cmd_motorIdle,
            .right = cmd_motorIdle
        };

        //variable for slow loop
        uint32_t timestamp_SlowLoopLastRun = 0;

        //variables for detecting timeout (switch to idle, after inactivity)
        float dutyLeft_lastActivity = 0;
        float dutyRight_lastActivity = 0;
        uint32_t timestamp_lastActivity = 0;
};


