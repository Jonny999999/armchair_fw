extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
}

#include "config.hpp"
#include "control.hpp"


//tag for logging
static const char * TAG = "control";

const char* controlModeStr[6] = {"IDLE", "JOYSTICK", "MASSAGE", "MQTT", "BLUETOOTH", "AUTO"};

//-----------------------------
//-------- constructor --------
//-----------------------------
controlledArmchair::controlledArmchair (
        buzzer_t * buzzer_f,
        controlledMotor* motorLeft_f,
        controlledMotor* motorRight_f
        ){

    //copy object pointers
    buzzer = buzzer_f;
    motorLeft = motorLeft_f;
    motorRight = motorRight_f;
    //TODO declare / configure controlled motors here instead of config (unnecessary that button object is globally available - only used here)?
}



//----------------------------------
//---------- Handle loop -----------
//----------------------------------
//function that repeatedly generates motor commands depending on the current mode
void controlledArmchair::startHandleLoop() {
    while (1){
        vTaskDelay(20 / portTICK_PERIOD_MS);

        ESP_LOGV(TAG, "control task executing... mode=%s", controlModeStr[(int)mode]);

        switch(mode) {
            default:
                mode = controlMode_t::IDLE;
                break;

            case controlMode_t::IDLE:
                motorRight->setTarget(motorstate_t::IDLE, 0); 
                motorLeft->setTarget(motorstate_t::IDLE, 0); 
                break;

            case controlMode_t::JOYSTICK:
                commands = joystick_generateCommandsDriving(joystick);
                motorRight->setTarget(commands.right.state, commands.right.duty); 
                motorLeft->setTarget(commands.left.state, commands.left.duty); 
                //TODO make motorctl.setTarget also accept motorcommand struct directly
                break;

            case controlMode_t::MASSAGE:
                motorRight->setTarget(motorstate_t::IDLE, 0); 
                motorLeft->setTarget(motorstate_t::IDLE, 0); 
                //TODO add actual command generation here
                break;

                //TODO: add other modes here
        }
    }
}



//-----------------------------------
//----------- changeMode ------------
//-----------------------------------
//function to change to a specified control mode
void controlledArmchair::changeMode(controlMode_t modeNew) {
    ESP_LOGW(TAG, "changing mode from %s to %s", controlModeStr[(int)mode], controlModeStr[(int)modeNew]);
    mode = modeNew;
    //TODO: add mutex
}



//-----------------------------------
//----------- toggleIdle ------------
//-----------------------------------
//function to toggle between IDLE and previous active mode
void controlledArmchair::toggleIdle() {
    if (mode == controlMode_t::IDLE){
        mode = modePrevious; //restore previous mode, or default if not switched yet
        buzzer->beep(2, 200, 100);
        ESP_LOGW(TAG, "switched mode from IDLE to %s", controlModeStr[(int)mode]);
    } else {
        modePrevious = mode; //store current mode
        mode = controlMode_t::IDLE; //set mode to IDLE
        buzzer->beep(1, 1000, 0);
        ESP_LOGW(TAG, "switched mode from IDLE to %s", controlModeStr[(int)mode]);
    }
}



//------------------------------------
//----------- toggleModes ------------
//------------------------------------
//function to toggle between two modes, but prefer first argument if entirely different mode is currently active
void controlledArmchair::toggleModes(controlMode_t modePrimary, controlMode_t modeSecondary) {

    //switch to secondary mode when primary is already active
    if (mode == modePrimary){
        ESP_LOGW(TAG, "toggleModes: switching from primaryMode %s to secondarMode %s", controlModeStr[(int)mode], controlModeStr[(int)modeSecondary]);
        buzzer->beep(2,200,100);
        mode = modeSecondary; //switch to secondary mode
    } 
    //switch to primary mode when any other mode is active
    else {
        ESP_LOGW(TAG, "toggleModes: switching from %s to primary mode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrimary]);
        buzzer->beep(4,200,100);
        mode = modePrimary;
    }
}
