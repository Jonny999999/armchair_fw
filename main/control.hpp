#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "buzzer.hpp"


//enum that decides how the motors get controlled
enum class controlMode_t {IDLE, JOYSTICK, MASSAGE, HTTP, MQTT, BLUETOOTH, AUTO};
//extern controlMode_t mode;
extern const char* controlModeStr[7];



//==================================
//========= control class ==========
//==================================
//controls the mode the armchair operates
//repeatedly generates the motor commands corresponding to current mode and sends those to motorcontrol
class controlledArmchair {
    public:
        //--- constructor ---
        controlledArmchair (
                buzzer_t* buzzer_f,
                controlledMotor* motorLeft_f,
                controlledMotor* motorRight_f
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

    private:

        //--- objects ---
        buzzer_t* buzzer;
        controlledMotor* motorLeft;
        controlledMotor* motorRight;

        //---variables ---
        //struct for motor commands returned by generate functions of each mode
        motorCommands_t commands;

        //definition of mode enum
        controlMode_t mode = controlMode_t::IDLE;

        //variable to store mode when toggling IDLE mode 
        controlMode_t modePrevious = controlMode_t::JOYSTICK; //default mode
};


