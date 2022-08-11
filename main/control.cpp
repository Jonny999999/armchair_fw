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


//tag for logging
static const char * TAG = "control";

const char* controlModeStr[7] = {"IDLE", "JOYSTICK", "MASSAGE", "HTTP", "MQTT", "BLUETOOTH", "AUTO"};

//-----------------------------
//-------- constructor --------
//-----------------------------
controlledArmchair::controlledArmchair (
        control_config_t config_f,
        buzzer_t * buzzer_f,
        controlledMotor* motorLeft_f,
        controlledMotor* motorRight_f,
        evaluatedJoystick* joystick_f,
        httpJoystick* httpJoystick_f
        ){

    //copy configuration
    config = config_f;
    //copy object pointers
    buzzer = buzzer_f;
    motorLeft = motorLeft_f;
    motorRight = motorRight_f;
    joystick_l = joystick_f,
    httpJoystickMain_l = httpJoystick_f;
    //set default mode from config
    modePrevious = config.defaultMode;
    
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
                break;


            case controlMode_t::IDLE:
                //copy preset commands for idling both motors
                commands = cmds_bothMotorsIdle;
                motorRight->setTarget(commands.right.state, commands.right.duty); 
                motorLeft->setTarget(commands.left.state, commands.left.duty); 
                vTaskDelay(200 / portTICK_PERIOD_MS);
                break;


            case controlMode_t::JOYSTICK:
                vTaskDelay(20 / portTICK_PERIOD_MS);
                //get current joystick data with getData method of evaluatedJoystick
                stickData = joystick_l->getData();
                //additionaly scale coordinates (more detail in slower area)
                joystick_scaleCoordinatesLinear(&stickData, 0.6, 0.35); //TODO: add scaling parameters to config
                //generate motor commands
                commands = joystick_generateCommandsDriving(stickData, altStickMapping);
                //apply motor commands
                motorRight->setTarget(commands.right.state, commands.right.duty); 
                motorLeft->setTarget(commands.left.state, commands.left.duty); 
                //TODO make motorctl.setTarget also accept motorcommand struct directly
                break;


            case controlMode_t::MASSAGE:
                vTaskDelay(10 / portTICK_PERIOD_MS);
                //--- read joystick ---
                //only update joystick data when input not frozen
                if (!freezeInput){
                    stickData = joystick_l->getData();
                }
                //--- generate motor commands ---
                //pass joystick data from getData method of evaluatedJoystick to generateCommandsShaking function
                commands = joystick_generateCommandsShaking(stickData);
                //apply motor commands
                motorRight->setTarget(commands.right.state, commands.right.duty); 
                motorLeft->setTarget(commands.left.state, commands.left.duty); 

                break;


            case controlMode_t::HTTP:
                //--- get joystick data from queue ---
                //Note this function waits several seconds (httpconfig.timeoutMs) for data to arrive, otherwise Center data or NULL is returned
                //TODO: as described above, when changing modes it might delay a few seconds for the change to apply
                stickData = httpJoystickMain_l->getData();
                //scale coordinates additionally (more detail in slower area)
                joystick_scaleCoordinatesLinear(&stickData, 0.6, 0.4); //TODO: add scaling parameters to config
                ESP_LOGD(TAG, "generating commands from x=%.3f  y=%.3f  radius=%.3f  angle=%.3f", stickData.x, stickData.y, stickData.radius, stickData.angle);
                //--- generate motor commands ---
                //Note: timeout (no data received) is handled in getData method
                commands = joystick_generateCommandsDriving(stickData, altStickMapping);

                //--- apply commands to motors ---
                //TODO make motorctl.setTarget also accept motorcommand struct directly
                motorRight->setTarget(commands.right.state, commands.right.duty); 
                motorLeft->setTarget(commands.left.state, commands.left.duty); 
               break;


            case controlMode_t::AUTO:
                vTaskDelay(20 / portTICK_PERIOD_MS);
               //generate commands
               commands = armchair.generateCommands();
               break;


              //  //TODO: add other modes here
        }


        //--- run actions based on received button button event ---
        //TODO: what if variable gets set from other task during this code? -> mutex around this code
        switch (buttonCount) {
            case 1: //define joystick center or freeze input
                if (mode == controlMode_t::JOYSTICK){
                    //joystick mode: calibrate joystick
                    joystick_l->defineCenter();
                } else if (mode == controlMode_t::MASSAGE){
                    //massage mode: toggle freeze of input (lock joystick at current values)
                    freezeInput = !freezeInput;
                    if (freezeInput){
                        buzzer->beep(5, 40, 25);
                    } else {
                        buzzer->beep(1, 300, 100);
                    }
                }
                break;

            case 12: //toggle alternative joystick mapping (reverse swapped) 
                altStickMapping = !altStickMapping;
                if (altStickMapping){
                    buzzer->beep(6, 70, 50);
                } else {
                    buzzer->beep(1, 500, 100);
                }
                break;
        }
        //--- reset button event --- (only one action per run)
        if (buttonCount > 0){
            ESP_LOGI(TAG, "resetting button event/count");
            buttonCount = 0;
        }



        //-----------------------
        //------ slow loop ------
        //-----------------------
        //this section is run about every 5s (+500ms)
        if (esp_log_timestamp() - timestamp_SlowLoopLastRun > 5000) {
            ESP_LOGV(TAG, "running slow loop... time since last run: %.1fs", (float)(esp_log_timestamp() - timestamp_SlowLoopLastRun)/1000);
            timestamp_SlowLoopLastRun = esp_log_timestamp();

            //run function which detects timeout (switch to idle)
            handleTimeout();
        }

    }//end while(1)
}//end startHandleLoop



//-----------------------------------
//---------- resetTimeout -----------
//-----------------------------------
void controlledArmchair::resetTimeout(){
    //TODO mutex
    timestamp_lastActivity = esp_log_timestamp();
}



//------------------------------------
//--------- sendButtonEvent ----------
//------------------------------------
void controlledArmchair::sendButtonEvent(uint8_t count){
    //TODO mutex - if not replaced with queue
    ESP_LOGI(TAG, "setting button event");
    buttonCount = count;
}



//------------------------------------
//---------- handleTimeout -----------
//------------------------------------
//percentage the duty can vary since last timeout check and still counts as incative 
//TODO: add this to config
float inactivityTolerance = 10; 

//local function that checks whether two values differ more than a given tolerance
bool validateActivity(float dutyOld, float dutyNow, float tolerance){
    float dutyDelta = dutyNow - dutyOld;
    if (fabs(dutyDelta) < tolerance) {
        return false; //no significant activity detected
    } else {
        return true; //there was activity
    }
}

//function that evaluates whether there is no activity/change on the motor duty for a certain time. If so, a switch to IDLE is issued. - has to be run repeatedly in a slow interval
void controlledArmchair::handleTimeout(){
    //check for timeout only when not idling already
    if (mode != controlMode_t::IDLE) {
        //get current duty from controlled motor objects
        float dutyLeftNow = motorLeft->getStatus().duty;
        float dutyRightNow = motorRight->getStatus().duty;

        //activity detected on any of the two motors
        if (validateActivity(dutyLeft_lastActivity, dutyLeftNow, inactivityTolerance) 
                || validateActivity(dutyRight_lastActivity, dutyRightNow, inactivityTolerance)
           ){
            ESP_LOGD(TAG, "timeout check: [activity] detected since last check -> reset");
            //reset last duty and timestamp
            dutyLeft_lastActivity = dutyLeftNow;
            dutyRight_lastActivity = dutyRightNow;
            resetTimeout();
        }
        //no activity on any motor and msTimeout exceeded
        else if (esp_log_timestamp() - timestamp_lastActivity > config.timeoutMs){
            ESP_LOGI(TAG, "timeout check: [TIMEOUT], no activity for more than %.ds  -> switch to idle", config.timeoutMs/1000);
            //toggle to idle mode
            toggleIdle();
        }
        else {
            ESP_LOGD(TAG, "timeout check: [inactive], last activity %.1f s ago, timeout after %d s", (float)(esp_log_timestamp() - timestamp_lastActivity)/1000, config.timeoutMs/1000);
        }
    }
}



//-----------------------------------
//----------- changeMode ------------
//-----------------------------------
//function to change to a specified control mode
void controlledArmchair::changeMode(controlMode_t modeNew) {
    //reset timeout timer
    resetTimeout();

    //exit if target mode is already active
    if (mode == modeNew) {
        ESP_LOGE(TAG, "changeMode: Already in target mode '%s' -> nothing to change", controlModeStr[(int)mode]);
        return;
    }

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

        case controlMode_t::MASSAGE:
            ESP_LOGW(TAG, "switching from MASSAGE mode -> restoring fading, reset frozen input");
            //TODO: fix issue when downfading was disabled before switching to massage mode - currently it gets enabled again here...
            //enable downfading (set to default value)
            motorLeft->setFade(fadeType_t::DECEL, true);
            motorRight->setFade(fadeType_t::DECEL, true);
            //set upfading to default value
            motorLeft->setFade(fadeType_t::ACCEL, true);
            motorRight->setFade(fadeType_t::ACCEL, true);
            //reset frozen input state
            freezeInput = false;
            break;
    }


    //--- run functions when changing TO certain mode ---
    switch(modeNew){
        default:
            ESP_LOGI(TAG, "noting to execute when changing TO this mode");
            break;

        case controlMode_t::IDLE:
            buzzer->beep(1, 1500, 0);
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

        case controlMode_t::MASSAGE:
            ESP_LOGW(TAG, "switching to MASSAGE mode -> reducing fading");
            uint32_t shake_msFadeAccel = 500; //TODO: move this to config

            //disable downfading (max. deceleration)
            motorLeft->setFade(fadeType_t::DECEL, false);
            motorRight->setFade(fadeType_t::DECEL, false);
            //reduce upfading (increase acceleration)
            motorLeft->setFade(fadeType_t::ACCEL, shake_msFadeAccel);
            motorRight->setFade(fadeType_t::ACCEL, shake_msFadeAccel);
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
        ESP_LOGW(TAG, "toggle idle: switched mode from IDLE to %s", controlModeStr[(int)mode]);
    } else {
        modePrevious = mode; //store current mode
        changeMode(controlMode_t::IDLE); //set mode to IDLE
        ESP_LOGW(TAG, "toggle idle: switched mode from %s to IDLE", controlModeStr[(int)mode]);
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
