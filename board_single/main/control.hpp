#pragma once

extern "C"
{
#include "nvs_flash.h"
#include "nvs.h"
}
#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "buzzer.hpp"
#include "http.hpp"
#include "auto.hpp"
#include "speedsensor.hpp"
#include "chairAdjust.hpp"

//percentage stick has to be moved in the opposite driving direction of current motor direction for braking to start
#define BRAKE_START_STICK_PERCENTAGE 95

//--------------------------------------------
//---- struct, enum, variable declarations ---
//--------------------------------------------
//enum that decides how the motors get controlled
enum class controlMode_t {IDLE, JOYSTICK, MASSAGE, HTTP, MQTT, BLUETOOTH, AUTO, ADJUST_CHAIR, MENU_SETTINGS, MENU_MODE_SELECT};
//string array representing the mode enum (for printing the state as string)
extern const char* controlModeStr[10];
extern const uint8_t controlModeMaxCount;

//--- control_config_t ---
//struct with config parameters
typedef struct control_config_t {
    controlMode_t defaultMode;  //default mode after startup and toggling IDLE
    bool idleAfterStartup;  //when true: armchair is in IDLE mode after startup (2x press switches to defaultMode)
                            //when false: immediately switches to active defaultMode after startup when set to false
    //timeout options
    uint32_t timeoutSwitchToIdleMs;         //time of inactivity after which the mode gets switched to IDLE
    uint32_t timeoutNotifyPowerStillOnMs;
} control_config_t;


//==========================
//==== controlModeToStr ====
//==========================
// convert controlMode enum or index to string for logging
const char * controlModeToStr(controlMode_t mode);
const char * controlModeToStr(int modeIndex);


//=======================================
//============ control task =============
//=======================================
//task that controls the armchair modes and initiates commands generation and applies them to driver
//parameter: pointer to controlledArmchair object 
void task_control( void * controlledArmchair );



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
                joystickGenerateCommands_config_t* joystickGenerateCommands_config_f,
                httpJoystick* httpJoystick_f,
                automatedArmchair_c* automatedArmchair,
                cControlledRest * legRest,
                cControlledRest * backRest,
                nvs_handle_t * nvsHandle_f
                );

        //--- functions ---
        //endless loop that repeatedly calls handle() and handleTimeout() methods respecting mutex
        void startHandleLoop();

        //function that changes to a specified control mode
        void changeMode(controlMode_t modeNew, bool noBeep = false);

        //function that toggle between IDLE and previous active mode (or default if not switched to certain mode yet)
        void toggleIdle();

        //function that toggles between two modes, but prefers first argument if entirely different mode is currently active
        void toggleModes(controlMode_t modePrimary, controlMode_t modeSecondary);

        //toggle between certain mode and previous mode
        void toggleMode(controlMode_t modePrimary);

        //function that restarts timer which initiates the automatic timeout (switch to IDLE) after certain time of inactivity
        void resetTimeout();

        //methods to get the current or previous control mode
        controlMode_t getCurrentMode() const {return mode;};
        controlMode_t getPreviousMode() const {return modePrevious;};
        const char *getCurrentModeStr() const { return controlModeStr[(int)mode]; };

        //--- mode specific ---
        // releases or locks joystick in place when in massage mode, returns true when input is frozen
        bool toggleFreezeInputMassage();
        // toggle between normal and alternative stick mapping (joystick reverse position inverted), returns true when alt mapping is active
        bool toggleAltStickMapping();

        // configure max dutycycle (in joystick or http mode)
        void setMaxDuty(float maxDutyNew) { 
            writeMaxDuty(maxDutyNew);
            motorLeft->setBrakeStartThresholdDuty(joystickGenerateCommands_config.maxDutyStraight * BRAKE_START_STICK_PERCENTAGE/100);
            motorRight->setBrakeStartThresholdDuty(joystickGenerateCommands_config.maxDutyStraight * BRAKE_START_STICK_PERCENTAGE/100);
        };
        float getMaxDuty() const {return joystickGenerateCommands_config.maxDutyStraight; };
        // configure max boost (in joystick or http mode)
        void setMaxRelativeBoostPer(float newValue) { joystickGenerateCommands_config.maxRelativeBoostPercentOfMaxDuty = newValue; };
        float getMaxRelativeBoostPer() const {return joystickGenerateCommands_config.maxRelativeBoostPercentOfMaxDuty; };

        uint32_t getInactivityDurationMs() {return esp_log_timestamp() - timestamp_lastActivity;};

    private:

        //--- functions ---
        //generate motor commands or run actions depending on the current mode
        void handle();

        //function that evaluates whether there is no activity/change on the motor duty for a certain time, if so a switch to IDLE is issued. - has to be run repeatedly in a slow interval
        void handleTimeout();

        void loadMaxDuty(); //load stored value for maxDuty from nvs
        void writeMaxDuty(float newMaxDuty); //write new value for maxDuty to nvs

        void idleBothMotors(); //turn both motors off

        //--- objects ---
        buzzer_t* buzzer;
        controlledMotor* motorLeft;
        controlledMotor* motorRight;
        httpJoystick* httpJoystickMain_l;
        evaluatedJoystick* joystick_l;
        joystickGenerateCommands_config_t joystickGenerateCommands_config;
        automatedArmchair_c *automatedArmchair;
        cControlledRest * legRest;
        cControlledRest * backRest;
        //handle for using the nvs flash (persistent config variables)
        nvs_handle_t * nvsHandle;

        //--- constants ---
        //command preset for idling motors
        const motorCommand_t cmd_motorIdle = {
            .state = motorstate_t::IDLE,
            .duty = 0
        };
        const motorCommands_t cmds_bothMotorsIdle = {
            .left = cmd_motorIdle,
            .right = cmd_motorIdle
        };
        const joystickData_t joystickData_center = {
            .position = joystickPos_t::CENTER,
            .x = 0,
            .y = 0,
            .radius = 0,
            .angle = 0
        };

        //---variables ---
        //struct for motor commands returned by generate functions of each mode
        motorCommands_t commands = cmds_bothMotorsIdle;
        //struct with config parameters
        control_config_t config;

        //mutex to prevent race condition between handle() and changeMode()
        SemaphoreHandle_t handleIteration_mutex;

        //store joystick data
        joystickData_t stickData = joystickData_center;
        joystickData_t stickDataLast = joystickData_center;

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

        //variable for slow loop
        uint32_t timestamp_SlowLoopLastRun = 0;

        //variables for detecting timeout (switch to idle, or notify "forgot to turn off" after inactivity
        uint32_t timestamp_lastModeChange = 0;
        uint32_t timestamp_lastActivity = 0;
        uint32_t timestamp_lastTimeoutBeep = 0;
};


