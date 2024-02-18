#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "buzzer.hpp"
#include "http.hpp"
#include "auto.hpp"
#include "speedsensor.hpp"
#include "chairAdjust.hpp"


//--------------------------------------------
//---- struct, enum, variable declarations ---
//--------------------------------------------
//enum that decides how the motors get controlled
enum class controlMode_t {IDLE, JOYSTICK, MASSAGE, HTTP, MQTT, BLUETOOTH, AUTO, ADJUST_CHAIR, MENU};
//string array representing the mode enum (for printing the state as string)
extern const char* controlModeStr[9];

//--- control_config_t ---
//struct with config parameters
typedef struct control_config_t {
    controlMode_t defaultMode;  //default mode after startup and toggling IDLE
    //timeout options
    uint32_t timeoutMs;         //time of inactivity after which the mode gets switched to IDLE
    float timeoutTolerancePer;  //percentage the duty can vary between timeout checks considered still inactive
} control_config_t;


//=======================================
//============ control task =============
//=======================================
//task that controls the armchair modes and initiates commands generation and applies them to driver
//parameter: pointer to controlledArmchair object 
void task_control( void * pvParameters );



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
                httpJoystick* httpJoystick_f,
                automatedArmchair_c* automatedArmchair,
                cControlledRest * legRest,
                cControlledRest * backRest
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

        //toggle between certain mode and previous mode
        void toggleMode(controlMode_t modePrimary);

        //function that restarts timer which initiates the automatic timeout (switch to IDLE) after certain time of inactivity
        void resetTimeout();

        //methods to get the current control mode
        controlMode_t getCurrentMode() const {return mode;};
        const char *getCurrentModeStr() const { return controlModeStr[(int)mode]; };

        // releases or locks joystick in place when in massage mode, returns true when input is frozen
        bool toggleFreezeInputMassage();

        // toggle between normal and alternative stick mapping (joystick reverse position inverted), returns true when alt mapping is active
        bool toggleAltStickMapping();

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
        automatedArmchair_c *automatedArmchair;
        cControlledRest * legRest;
        cControlledRest * backRest;

        //---variables ---
        //struct for motor commands returned by generate functions of each mode
        motorCommands_t commands;
        //struct with config parameters
        control_config_t config;

        //store joystick data
        joystickData_t stickData;
        bool altStickMapping; //alternative joystick mapping (reverse mapped differently)

        //variables for http mode
        uint32_t http_timestamp_lastData = 0;

        //variables for MASSAGE mode
        bool freezeInput = false;

        //variables for AUTO mode
        auto_instruction_t instruction = auto_instruction_t::NONE; //variable to receive instructions from automatedArmchair_c
        
        //variable to store button event
        uint8_t buttonCount = 0;

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


