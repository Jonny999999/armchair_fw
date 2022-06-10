extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
}

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "joystick.hpp"
#include "config.hpp"
#include "control.hpp"


//=================================
//===== variable declaration ======
//=================================
//tag for logging
static const char * TAG = "control";

//definition of mode enum
controlMode_t mode = controlMode_t::IDLE;
const char* controlModeStr[6] = {"IDLE", "JOYSTICK", "MASSAGE", "MQTT", "BLUETOOTH", "AUTO"};



//==================================
//========== handle task ===========
//==================================
//task that repeatedly generates motor commands depending on the current mode
void task_control ( void * pvParameters ) {
    while (1){
        vTaskDelay(20 / portTICK_PERIOD_MS);

        ESP_LOGV(TAG, "control task executing... mode=%s", controlModeStr[(int)mode]);

        switch(mode) {
            default:
                mode = controlMode_t::IDLE;
                break;

            case controlMode_t::IDLE:
                motorRight.setTarget(motorstate_t::IDLE, 0); 
                motorLeft.setTarget(motorstate_t::IDLE, 0); 
                break;

            case controlMode_t::JOYSTICK:
                motorCommands_t commands = joystick_generateCommandsDriving(joystick);
                motorRight.setTarget(commands.right.state, commands.right.duty); 
                motorLeft.setTarget(commands.left.state, commands.left.duty); 
                //TODO make motorctl.setTarget also accept motorcommand struct directly
                break;

                //TODO: add other modes here
        }
    }
}



//===================================
//=========== changeMode ============
//===================================
//function to change to a specified control mode
void control_changeMode(controlMode_t modeNew) {
    ESP_LOGW(TAG, "changing mode from %s to %s", controlModeStr[(int)mode], controlModeStr[(int)modeNew]);
    mode = modeNew;
    //TODO: add mutex
}



