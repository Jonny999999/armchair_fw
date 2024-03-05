extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/queue.h"

//custom C libraries
#include "wifi.h"
}

#include "config.h"
#include "control.hpp"
#include "chairAdjust.hpp"
#include "display.hpp" // needed for getBatteryPercent()


//used definitions moved from config.h:
//#define JOYSTICK_LOG_IN_IDLE


//tag for logging
static const char * TAG = "control";
const char* controlModeStr[9] = {"IDLE", "JOYSTICK", "MASSAGE", "HTTP", "MQTT", "BLUETOOTH", "AUTO", "ADJUST_CHAIR", "MENU"};


//-----------------------------
//-------- constructor --------
//-----------------------------
controlledArmchair::controlledArmchair(
    control_config_t config_f,
    buzzer_t *buzzer_f,
    controlledMotor *motorLeft_f,
    controlledMotor *motorRight_f,
    evaluatedJoystick *joystick_f,
    joystickGenerateCommands_config_t *joystickGenerateCommands_config_f,
    httpJoystick *httpJoystick_f,
    automatedArmchair_c *automatedArmchair_f,
    cControlledRest *legRest_f,
    cControlledRest *backRest_f,
    nvs_handle_t * nvsHandle_f)
{

    //copy configuration
    config = config_f;
    joystickGenerateCommands_config = *joystickGenerateCommands_config_f;
    //copy object pointers
    buzzer = buzzer_f;
    motorLeft = motorLeft_f;
    motorRight = motorRight_f;
    joystick_l = joystick_f,
    httpJoystickMain_l = httpJoystick_f;
    automatedArmchair = automatedArmchair_f;
    legRest = legRest_f;
    backRest = backRest_f;
    nvsHandle = nvsHandle_f;
    //set default mode from config
    modePrevious = config.defaultMode;
    
    // override default config value if maxDuty is found in nvs
    loadMaxDuty();
}


//=======================================
//============ control task =============
//=======================================
// task that controls the armchair modes
// generates commands depending on current mode and sends those to corresponding task
// parameter: pointer to controlledArmchair object
void task_control( void * pvParameters ){
    controlledArmchair * control = (controlledArmchair *)pvParameters;
    ESP_LOGW(TAG, "Initializing controlledArmchair and starting handle loop");
    control->startHandleLoop();
}


//----------------------------------
//---------- Handle loop -----------
//----------------------------------
//function that repeatedly generates motor commands depending on the current mode
void controlledArmchair::startHandleLoop() {
    while (1){
        ESP_LOGV(TAG, "control loop executing... mode=%s", controlModeStr[(int)mode]);

        switch(mode) {
            default:
                mode = controlMode_t::IDLE;
                break;

            case controlMode_t::IDLE:
                //copy preset commands for idling both motors - now done once at mode change
                //commands = cmds_bothMotorsIdle;
                //motorRight->setTarget(commands.right.state, commands.right.duty); 
                //motorLeft->setTarget(commands.left.state, commands.left.duty); 
                vTaskDelay(500 / portTICK_PERIOD_MS);
#ifdef JOYSTICK_LOG_IN_IDLE
                // get joystick data and log it
                joystickData_t data joystick_l->getData();
                ESP_LOGI("JOYSTICK_LOG_IN_IDLE", "x=%.3f, y=%.3f, radius=%.3f, angle=%.3f, pos=%s, adcx=%d, adcy=%d",
                         data.x, data.y, data.radius, data.angle,
                         joystickPosStr[(int)data.position],
                         objects->joystick->getRawX(), objects->joystick->getRawY());
#endif
        break;
            //------- handle JOYSTICK mode -------
            case controlMode_t::JOYSTICK:
                vTaskDelay(50 / portTICK_PERIOD_MS);
                //get current joystick data with getData method of evaluatedJoystick
                stickDataLast = stickData;
                stickData = joystick_l->getData();
                //additionaly scale coordinates (more detail in slower area)
                joystick_scaleCoordinatesLinear(&stickData, 0.6, 0.35); //TODO: add scaling parameters to config
                // generate motor commands
                // only generate when the stick data actually changed (e.g. stick stayed in center)
                if (stickData.x != stickDataLast.x || stickData.y != stickDataLast.y)
                {
                    resetTimeout(); //user input -> reset switch to IDLE timeout
                    commands = joystick_generateCommandsDriving(stickData, &joystickGenerateCommands_config);
                    // apply motor commands
                    motorRight->setTarget(commands.right);
                    motorLeft->setTarget(commands.left);
                }
                else
                {
                    vTaskDelay(20 / portTICK_PERIOD_MS);
                    ESP_LOGV(TAG, "analog joystick data unchanged at %s not updating commands", joystickPosStr[(int)stickData.position]);
                }
                break;


            //------- handle MASSAGE mode -------
            case controlMode_t::MASSAGE:
                vTaskDelay(10 / portTICK_PERIOD_MS);
                //--- read joystick ---
                // only update joystick data when input not frozen
                stickDataLast = stickData;
                if (!freezeInput)
                    stickData = joystick_l->getData();
                //--- generate motor commands ---
                // only generate when the stick data actually changed (e.g. stick stayed in center)
                if (stickData.x != stickDataLast.x || stickData.y != stickDataLast.y)
                {
                    resetTimeout(); // user input -> reset switch to IDLE timeout
                    // pass joystick data from getData method of evaluatedJoystick to generateCommandsShaking function
                    commands = joystick_generateCommandsShaking(stickData);
                    // apply motor commands
                    motorRight->setTarget(commands.right);
                    motorLeft->setTarget(commands.left);
                }
                break;


            //------- handle HTTP mode -------
            case controlMode_t::HTTP:
                //--- get joystick data from queue ---
                stickDataLast = stickData;
                stickData = httpJoystickMain_l->getData(); //get last stored data from receive queue (waits up to 500ms for new event to arrive)
                //scale coordinates additionally (more detail in slower area)
                joystick_scaleCoordinatesLinear(&stickData, 0.6, 0.4); //TODO: add scaling parameters to config
                ESP_LOGD(TAG, "generating commands from x=%.3f  y=%.3f  radius=%.3f  angle=%.3f", stickData.x, stickData.y, stickData.radius, stickData.angle);
                //--- generate motor commands ---
                //only generate when the stick data actually changed (e.g. no new data recevied via http)
                if (stickData.x != stickDataLast.x || stickData.y != stickDataLast.y ){
                    resetTimeout(); // user input -> reset switch to IDLE timeout
                    // Note: timeout (no data received) is handled in getData method
                    commands = joystick_generateCommandsDriving(stickData, &joystickGenerateCommands_config);

                    //--- apply commands to motors ---
                    motorRight->setTarget(commands.right);
                    motorLeft->setTarget(commands.left);
                }
                else
                {
                    ESP_LOGD(TAG, "http joystick data unchanged at %s not updating commands", joystickPosStr[(int)stickData.position]);
                }
                break;


            //------- handle AUTO mode -------
            case controlMode_t::AUTO:
                vTaskDelay(20 / portTICK_PERIOD_MS);
               //generate commands
               commands = automatedArmchair->generateCommands(&instruction);
                //--- apply commands to motors ---
               motorRight->setTarget(commands.right); 
               motorLeft->setTarget(commands.left); 

               //process received instruction
               switch (instruction) {
                   case auto_instruction_t::NONE:
                       break;
                   case auto_instruction_t::SWITCH_PREV_MODE:
                       toggleMode(controlMode_t::AUTO);
                       break;
                   case auto_instruction_t::SWITCH_JOYSTICK_MODE:
                       changeMode(controlMode_t::JOYSTICK);
                       break;
                   case auto_instruction_t::RESET_ACCEL_DECEL:
                       //enable downfading (set to default value)
                       motorLeft->setFade(fadeType_t::DECEL, true);
                       motorRight->setFade(fadeType_t::DECEL, true);
                       //set upfading to default value
                       motorLeft->setFade(fadeType_t::ACCEL, true);
                       motorRight->setFade(fadeType_t::ACCEL, true);
                       break;
                   case auto_instruction_t::RESET_ACCEL:
                       //set upfading to default value
                       motorLeft->setFade(fadeType_t::ACCEL, true);
                       motorRight->setFade(fadeType_t::ACCEL, true);
                       break;
                   case auto_instruction_t::RESET_DECEL:
                       //enable downfading (set to default value)
                       motorLeft->setFade(fadeType_t::DECEL, true);
                       motorRight->setFade(fadeType_t::DECEL, true);
                       break;
               }
               break;


            //------- handle ADJUST_CHAIR mode -------
            case controlMode_t::ADJUST_CHAIR:
                vTaskDelay(100 / portTICK_PERIOD_MS);
                //--- read joystick ---
                stickDataLast = stickData;
                stickData = joystick_l->getData();
                //--- control armchair position with joystick input ---
                // dont update when stick data did not change
                if (stickData.x != stickDataLast.x || stickData.y != stickDataLast.y)
                {
                    resetTimeout(); // user input -> reset switch to IDLE timeout
                    controlChairAdjustment(joystick_l->getData(), legRest, backRest);
                }
                break;


            //------- handle MENU mode -------
            case controlMode_t::MENU:
                //nothing to do here, display task handles the menu
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;

              //TODO: add other modes here
        }

        //-----------------------
        //------ slow loop ------
        //-----------------------
        //this section is run about every 5s (+500ms)
        if (esp_log_timestamp() - timestamp_SlowLoopLastRun > 5000) {
            ESP_LOGV(TAG, "running slow loop... time since last run: %.1fs", (float)(esp_log_timestamp() - timestamp_SlowLoopLastRun)/1000);
            timestamp_SlowLoopLastRun = esp_log_timestamp();
            //run function that detects timeout (switch to idle, or notify "forgot to turn off")
            handleTimeout();
        }

    }//end while(1)
}//end startHandleLoop




//---------------------------------------
//------ toggleFreezeInputMassage -------
//---------------------------------------
// releases or locks joystick in place when in massage mode
bool controlledArmchair::toggleFreezeInputMassage()
{
    if (mode == controlMode_t::MASSAGE)
    {
        // massage mode: toggle freeze of input (lock joystick at current values)
        freezeInput = !freezeInput;
        if (freezeInput)
        {
            buzzer->beep(5, 40, 25);
            ESP_LOGW(TAG, "joystick input is now locked in place");
        }
        else
        {
            buzzer->beep(1, 300, 100);
            ESP_LOGW(TAG, "joystick input gets updated again");
        }
        return freezeInput;
    }
    else
    {
        ESP_LOGE(TAG, "can not freeze/unfreeze joystick input - not in MASSAGE mode!");
        return 0;
    }
}



//-------------------------------------
//------- toggleAltStickMapping -------
//-------------------------------------
// toggle between normal and alternative stick mapping (joystick reverse position inverted)
bool controlledArmchair::toggleAltStickMapping()
{
    joystickGenerateCommands_config.altStickMapping = !joystickGenerateCommands_config.altStickMapping;
    if (joystickGenerateCommands_config.altStickMapping)
    {
        buzzer->beep(6, 70, 50);
        ESP_LOGW(TAG, "changed to alternative stick mapping");
    }
    else
    {
        buzzer->beep(1, 500, 100);
        ESP_LOGW(TAG, "changed to default stick mapping");
    }
    return joystickGenerateCommands_config.altStickMapping;
}


//-----------------------------------
//--------- idleBothMotors ----------
//-----------------------------------
// turn both motors off
void controlledArmchair::idleBothMotors(){
    motorRight->setTarget(cmd_motorIdle);
    motorLeft->setTarget(cmd_motorIdle);
}


//-----------------------------------
//---------- resetTimeout -----------
//-----------------------------------
void controlledArmchair::resetTimeout(){
    //TODO mutex
    timestamp_lastActivity = esp_log_timestamp();
    ESP_LOGV(TAG, "timeout: activity detected, resetting timeout");
}


//------------------------------------
//---------- handleTimeout -----------
//------------------------------------
// switch to IDLE when no activity (prevent accidential movement)
// notify "power still on" when in IDLE for a very long time (prevent battery drain when forgotten to turn off)
// this function has to be run repeatedly (can be slow interval)
#define TIMEOUT_POWER_STILL_ON_BEEP_INTERVAL_MS 5 * 60 * 1000 // beep every 5 minutes for someone to notice
#define TIMEOUT_POWER_STILL_ON_BATTERY_THRESHOLD_PERCENT 96 // only notify/beep when below certain percentage (prevent beeping when connected to charger)
// note: timeout durations are configured in config.cpp
void controlledArmchair::handleTimeout()
{
    uint32_t noActivityDurationMs = esp_log_timestamp() - timestamp_lastActivity;
    // log current inactivity and configured timeouts
    ESP_LOGD(TAG, "timeout check: last activity %dmin and %ds ago - timeout IDLE after %ds - notify after power on after %dh",
             noActivityDurationMs / 1000 / 60,
             noActivityDurationMs / 1000 % 60,
             config.timeoutSwitchToIdleMs / 1000,
             config.timeoutNotifyPowerStillOnMs / 1000 / 60 / 60);

    // -- timeout switch to IDLE --
    // timeout to IDLE when not idling already
    if (mode != controlMode_t::IDLE && noActivityDurationMs > config.timeoutSwitchToIdleMs)
    {
        ESP_LOGW(TAG, "timeout check: [TIMEOUT], no activity for more than %ds  -> switch to IDLE", config.timeoutSwitchToIdleMs / 1000);
        changeMode(controlMode_t::IDLE);
        //TODO switch to previous status-screen when activity detected
    }

    // -- timeout notify "forgot to turn off" --
    // repeatedly notify via buzzer when in IDLE for a very long time to prevent battery drain ("forgot to turn off")
    // also battery charge-level has to be below certain threshold to prevent beeping in case connected to charger
    // note: ignores user input while in IDLE (e.g. encoder rotation)
    else if ((esp_log_timestamp() - timestamp_lastModeChange) > config.timeoutNotifyPowerStillOnMs && getBatteryPercent() < TIMEOUT_POWER_STILL_ON_BATTERY_THRESHOLD_PERCENT)
    {
        // beep in certain intervals
        if ((esp_log_timestamp() - timestamp_lastTimeoutBeep) > TIMEOUT_POWER_STILL_ON_BEEP_INTERVAL_MS)
        {
            ESP_LOGW(TAG, "timeout: [TIMEOUT] in IDLE since %.3f hours -> beeping", (float)(esp_log_timestamp() - timestamp_lastModeChange) / 1000 / 60 / 60);
            // TODO dont beep at certain time ranges (e.g. at night)
            timestamp_lastTimeoutBeep = esp_log_timestamp();
            buzzer->beep(6, 100, 50);
        }
    }
}



//-----------------------------------
//----------- changeMode ------------
//-----------------------------------
//function to change to a specified control mode
void controlledArmchair::changeMode(controlMode_t modeNew) {

    //exit if target mode is already active
    if (mode == modeNew) {
        ESP_LOGE(TAG, "changeMode: Already in target mode '%s' -> nothing to change", controlModeStr[(int)mode]);
        return;
    }

    //copy previous mode
    modePrevious = mode;
    //store time changed (needed for timeout)
    timestamp_lastModeChange = esp_log_timestamp();

	ESP_LOGW(TAG, "=== changing mode from %s to %s ===", controlModeStr[(int)mode], controlModeStr[(int)modeNew]);

	//========== commands change FROM mode ==========
	//run functions when changing FROM certain mode
	switch(modePrevious){
		default:
			ESP_LOGI(TAG, "noting to execute when changing FROM this mode");
			break;

		case controlMode_t::IDLE:
#ifdef JOYSTICK_LOG_IN_IDLE
			ESP_LOGI(TAG, "disabling debug output for 'evaluatedJoystick'");
			esp_log_level_set("evaluatedJoystick", ESP_LOG_WARN); //FIXME: loglevel from config
#endif
            buzzer->beep(1,200,100);
			break;

        case controlMode_t::HTTP:
            ESP_LOGW(TAG, "switching from HTTP mode -> stopping wifi-ap");
            wifi_stop_ap();
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

        case controlMode_t::AUTO:
            ESP_LOGW(TAG, "switching from AUTO mode -> restoring fading to default");
            //TODO: fix issue when downfading was disabled before switching to auto mode - currently it gets enabled again here...
            //enable downfading (set to default value)
            motorLeft->setFade(fadeType_t::DECEL, true);
            motorRight->setFade(fadeType_t::DECEL, true);
            //set upfading to default value
            motorLeft->setFade(fadeType_t::ACCEL, true);
            motorRight->setFade(fadeType_t::ACCEL, true);
            break;

        case controlMode_t::ADJUST_CHAIR:
            ESP_LOGW(TAG, "switching from ADJUST_CHAIR mode => turning off adjustment motors...");
            //prevent motors from being always on in case of mode switch while joystick is not in center thus motors currently moving
            legRest->setState(REST_OFF);
            backRest->setState(REST_OFF);
            break;

    }


    //========== commands change TO mode ==========
    //run functions when changing TO certain mode
    switch(modeNew){
        default:
            ESP_LOGI(TAG, "noting to execute when changing TO this mode");
            break;

        case controlMode_t::IDLE:
            ESP_LOGW(TAG, "switching to IDLE mode: turning both motors off, beep");
            idleBothMotors();
            buzzer->beep(1, 900, 0);
            break;

        case controlMode_t::HTTP:
            ESP_LOGW(TAG, "switching to HTTP mode -> starting wifi-ap");
            wifi_start_ap();
            break;

        case controlMode_t::ADJUST_CHAIR:
            ESP_LOGW(TAG, "switching to ADJUST_CHAIR mode: turning both motors off, beep");
            idleBothMotors();
            buzzer->beep(3, 100, 50);
            break;

        case controlMode_t::MENU:
            idleBothMotors();
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


//TODO simplify the following 3 functions? can be replaced by one?

//-----------------------------------
//----------- toggleIdle ------------
//-----------------------------------
//function to toggle between IDLE and previous active mode
void controlledArmchair::toggleIdle() {
    //toggle between IDLE and previous mode
    toggleMode(controlMode_t::IDLE);
}



//------------------------------------
//----------- toggleModes ------------
//------------------------------------
//function to toggle between two modes, but prefer first argument if entirely different mode is currently active
void controlledArmchair::toggleModes(controlMode_t modePrimary, controlMode_t modeSecondary) {
    //switch to secondary mode when primary is already active
    if (mode == modePrimary){
        ESP_LOGW(TAG, "toggleModes: switching from primaryMode %s to secondarMode %s", controlModeStr[(int)mode], controlModeStr[(int)modeSecondary]);
        //buzzer->beep(2,200,100);
        changeMode(modeSecondary); //switch to secondary mode
    } 
    //switch to primary mode when any other mode is active
    else {
        ESP_LOGW(TAG, "toggleModes: switching from %s to primary mode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrimary]);
        //buzzer->beep(4,200,100);
        changeMode(modePrimary);
    }
}



//-----------------------------------
//----------- toggleMode ------------
//-----------------------------------
//function that toggles between certain mode and previous mode
void controlledArmchair::toggleMode(controlMode_t modePrimary){

    //switch to previous mode when primary is already active
    if (mode == modePrimary){
        ESP_LOGW(TAG, "toggleMode: switching from primaryMode %s to previousMode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrevious]);
        //buzzer->beep(2,200,100);
        changeMode(modePrevious); //switch to previous mode
    } 
    //switch to primary mode when any other mode is active
    else {
        ESP_LOGW(TAG, "toggleModes: switching from %s to primary mode %s", controlModeStr[(int)mode], controlModeStr[(int)modePrimary]);
        //buzzer->beep(4,200,100);
        changeMode(modePrimary);
    }
}




//-----------------------------
//-------- loadMaxDuty --------
//-----------------------------
// update local config value when maxDuty is stored in nvs
void controlledArmchair::loadMaxDuty(void)
{
    // default value is already loaded (constructor)
    // read from nvs
    uint16_t valueRead;
    esp_err_t err = nvs_get_u16(*nvsHandle, "c-maxDuty", &valueRead);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGW(TAG, "Successfully read value '%s' from nvs. Overriding default value %.2f with %.2f", "c-maxDuty", joystickGenerateCommands_config.maxDutyStraight, valueRead/100.0);
        joystickGenerateCommands_config.maxDutyStraight = (float)(valueRead/100.0);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "nvs: the value '%s' is not initialized yet, keeping default value %.2f", "c-maxDuty", joystickGenerateCommands_config.maxDutyStraight);
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading nvs!", esp_err_to_name(err));
    }
}


//-----------------------------------
//---------- writeMaxDuty -----------
//-----------------------------------
// write provided value to nvs to be persistent and update local variable in joystickGenerateCommmands_config struct
// note: duty percentage gets stored as uint with factor 100 (to get more precision)
void controlledArmchair::writeMaxDuty(float newValue){
    // check if unchanged
    if(joystickGenerateCommands_config.maxDutyStraight == newValue){
        ESP_LOGW(TAG, "value unchanged at %.2f, not writing to nvs", newValue);
        return;
    }
    // update nvs value
    ESP_LOGW(TAG, "updating nvs value '%s' from %.2f to %.2f", "c-maxDuty", joystickGenerateCommands_config.maxDutyStraight, newValue) ;
    esp_err_t err = nvs_set_u16(*nvsHandle, "c-maxDuty", (uint16_t)(newValue*100));
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed writing");
    err = nvs_commit(*nvsHandle);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed committing updates");
    else
        ESP_LOGI(TAG, "nvs: successfully committed updates");
    // update variable
    joystickGenerateCommands_config.maxDutyStraight = newValue;
}