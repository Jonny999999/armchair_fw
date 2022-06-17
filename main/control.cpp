extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/queue.h"


//custom C libraries
#include "wifi.h"
}

#include "config.hpp"
#include "control.hpp"
#include "http.hpp"


//tag for logging
static const char * TAG = "control";

const char* controlModeStr[7] = {"IDLE", "JOYSTICK", "MASSAGE", "HTTP", "MQTT", "BLUETOOTH", "AUTO"};

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

        ESP_LOGV(TAG, "control task executing... mode=%s", controlModeStr[(int)mode]);

        switch(mode) {
            default:
                mode = controlMode_t::IDLE;
                vTaskDelay(200 / portTICK_PERIOD_MS);
                break;

            case controlMode_t::IDLE:
                motorRight->setTarget(motorstate_t::IDLE, 0); 
                motorLeft->setTarget(motorstate_t::IDLE, 0); 
                vTaskDelay(200 / portTICK_PERIOD_MS);
                break;

            case controlMode_t::JOYSTICK:
                //generate motor commands
                //pass joystick data from getData method of evaluatedJoystick to generateCommandsDriving function
                commands = joystick_generateCommandsDriving(joystick.getData());
                //TODO: pass pointer to joystick object to control class instead of accessing it directly globally
                motorRight->setTarget(commands.right.state, commands.right.duty); 
                motorLeft->setTarget(commands.left.state, commands.left.duty); 
                //TODO make motorctl.setTarget also accept motorcommand struct directly
                vTaskDelay(20 / portTICK_PERIOD_MS);
                break;

            case controlMode_t::MASSAGE:
                motorRight->setTarget(motorstate_t::IDLE, 0); 
                motorLeft->setTarget(motorstate_t::IDLE, 0); 
                //TODO add actual command generation here
                vTaskDelay(20 / portTICK_PERIOD_MS);
                break;

            case controlMode_t::HTTP:
                //create emptry struct for receiving data from http function
                joystickData_t dataRead = { };

                //get joystick data from queue
                if( xQueueReceive( joystickDataQueue, &dataRead, pdMS_TO_TICKS(500) ) ) {
                    ESP_LOGD(TAG, "received data from http queue: x=%.3f  y=%.3f  radius=%.3f  angle=%.3f",
                            dataRead.x, dataRead.y, dataRead.radius, dataRead.angle);

                    //pass received joystick data from http queue to generatecommands function from joystick.hpp
                    commands = joystick_generateCommandsDriving(dataRead);
                    ESP_LOGD(TAG, "generated motor commands");
                    //apply commands to motor control objects
                    //TODO make motorctl.setTarget also accept motorcommand struct directly
                    motorRight->setTarget(commands.right.state, commands.right.duty); 
                    motorLeft->setTarget(commands.left.state, commands.left.duty); 

                }

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
    //copy previous mode
    controlMode_t modePrevious = mode;

    ESP_LOGW(TAG, "=== changing mode from %s to %s ===", controlModeStr[(int)mode], controlModeStr[(int)modeNew]);

    //--- run functions when changing FROM certain mode ---
    switch(modePrevious){
        default:
            ESP_LOGI(TAG, "noting to execute when changing FROM this mode");
            break;

        case controlMode_t::HTTP:
            ESP_LOGW(TAG, "switching from http mode -> disabling http and wifi");
            //stop http server
            ESP_LOGI(TAG, "disabling http server...");
            http_stop_server();


            //FIXME: make wifi function work here - currently starting wifi at startup (see notes main.cpp)
            //stop wifi
            //TODO: decide whether ap or client is currently used - which has to be disabled?
            //ESP_LOGI(TAG, "deinit wifi...");
            //wifi_deinit_client();
            //wifi_deinit_ap();
            ESP_LOGI(TAG, "done stopping http mode");
            break;
    }


    //--- run functions when changing TO certain mode ---
    switch(modeNew){
        default:
            ESP_LOGI(TAG, "noting to execute when changing TO this mode");
            break;

        case controlMode_t::HTTP:
            ESP_LOGW(TAG, "switching to http mode -> enabling http and wifi");
            //start wifi
            //TODO: decide wether ap or client should be started
            ESP_LOGI(TAG, "init wifi...");

            //FIXME: make wifi function work here - currently starting wifi at startup (see notes main.cpp)
            //wifi_init_client();
            //wifi_init_ap();

            //wait for wifi
            //ESP_LOGI(TAG, "waiting for wifi...");
            //vTaskDelay(1000 / portTICK_PERIOD_MS);

            //start http server
            ESP_LOGI(TAG, "init http server...");
            http_init_server();
            ESP_LOGI(TAG, "done initializing http mode");
            break;
    }

    //--- update mode to new mode ---
    //TODO: add mutex
    mode = modeNew;
}



//-----------------------------------
//----------- toggleIdle ------------
//-----------------------------------
//function to toggle between IDLE and previous active mode
void controlledArmchair::toggleIdle() {
    if (mode == controlMode_t::IDLE){
        changeMode(modePrevious); //restore previous mode, or default if not switched yet
        buzzer->beep(2, 200, 100);
        ESP_LOGW(TAG, "switched mode from IDLE to %s", controlModeStr[(int)mode]);
    } else {
        modePrevious = mode; //store current mode
        changeMode(controlMode_t::IDLE); //set mode to IDLE
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
        changeMode(modeSecondary); //switch to secondary mode
    } 
    //switch to primary mode when any other mode is active
    else {
        ESP_LOGW(TAG, "toggleModes: switching from %s to primary mode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrimary]);
        buzzer->beep(4,200,100);
        changeMode(modePrimary);
    }
}
