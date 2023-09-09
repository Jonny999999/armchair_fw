extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
}

#include "button.hpp"



//tag for logging
static const char * TAG = "button";



//-----------------------------
//-------- constructor --------
//-----------------------------
buttonCommands::buttonCommands(gpio_evaluatedSwitch * button_f, evaluatedJoystick * joystick_f, controlledArmchair * control_f, buzzer_t * buzzer_f, controlledMotor * motorLeft_f, controlledMotor * motorRight_f){
    //copy object pointers
    button = button_f;
    joystick = joystick_f;
    control = control_f;
    buzzer = buzzer_f;
    motorLeft = motorLeft_f;
    motorRight = motorRight_f;
    //TODO declare / configure evaluatedSwitch here instead of config (unnecessary that button object is globally available - only used here)?
}



//----------------------------
//--------- action -----------
//----------------------------
//function that runs commands depending on a count value
void buttonCommands::action (uint8_t count, bool lastPressLong){
    //--- variable declarations ---
    bool decelEnabled; //for different beeping when toggling
    commandSimple_t cmds[8]; //array for commands for automatedArmchair

    //--- get joystick position ---
    //joystickData_t stickData = joystick->getData();

    //--- actions based on count ---
    switch (count){
        //no such command
        default:
            ESP_LOGE(TAG, "no command for count=%d defined", count);
            buzzer->beep(3, 400, 100);
            break;

        case 1:
            //restart contoller when 1x long pressed
            if (lastPressLong){
                ESP_LOGW(TAG, "RESTART");
                buzzer->beep(1,1000,1);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                return;
            } 
			//note: disabled joystick calibration due to accidential trigger
//
//            ESP_LOGW(TAG, "cmd %d: sending button event to control task", count);
//            //-> define joystick center or toggle freeze input (executed in control task)
//            control->sendButtonEvent(count); //TODO: always send button event to control task (not just at count=1) -> control.cpp has to be changed
            break;
        case 2:
            //run automatic commands to lift leg support when pressed 1x short 1x long
            if (lastPressLong){
                //define commands
                cmds[0] =
                {
                    .motorCmds = {
                        .left = {motorstate_t::REV, 90},
                        .right = {motorstate_t::REV, 90}
                    },
                    .msDuration = 1200,
                    .fadeDecel = 800,
                    .fadeAccel = 1300,
                    .instruction = auto_instruction_t::NONE
                };
                cmds[1] =
                {
                    .motorCmds = {
                        .left = {motorstate_t::FWD, 70},
                        .right = {motorstate_t::FWD, 70}
                    },
                    .msDuration = 70,
                    .fadeDecel = 0,
                    .fadeAccel = 300,
                    .instruction = auto_instruction_t::NONE
                };
                cmds[2] =
                {
                    .motorCmds = {
                        .left = {motorstate_t::IDLE, 0},
                        .right = {motorstate_t::IDLE, 0}
                    },
                    .msDuration = 10,
                    .fadeDecel = 800,
                    .fadeAccel = 1300,
                    .instruction = auto_instruction_t::SWITCH_JOYSTICK_MODE
                };

                //send commands to automatedArmchair command queue
                armchair.addCommands(cmds, 3);

                //change mode to AUTO
                control->changeMode(controlMode_t::AUTO);
                return;
            }

            //toggle idle when 2x pressed
            ESP_LOGW(TAG, "cmd %d: toggle IDLE", count);
            control->toggleIdle(); //toggle between idle and previous/default mode
            break;


        case 3:
            ESP_LOGW(TAG, "cmd %d: switch to JOYSTICK", count);
            control->changeMode(controlMode_t::JOYSTICK); //switch to JOYSTICK mode
            break;

        case 4:
            ESP_LOGW(TAG, "cmd %d: toggle between HTTP and JOYSTICK", count);
            control->toggleModes(controlMode_t::HTTP, controlMode_t::JOYSTICK); //toggle between HTTP and JOYSTICK mode
            break;

        case 6:
            ESP_LOGW(TAG, "cmd %d: toggle between MASSAGE and JOYSTICK", count);
            control->toggleModes(controlMode_t::MASSAGE, controlMode_t::JOYSTICK); //toggle between MASSAGE and JOYSTICK mode
            break;

        case 8:
            //toggle deceleration fading between on and off
            //decelEnabled = motorLeft->toggleFade(fadeType_t::DECEL);
            //motorRight->toggleFade(fadeType_t::DECEL);
            decelEnabled = motorLeft->toggleFade(fadeType_t::ACCEL);
            motorRight->toggleFade(fadeType_t::ACCEL);
            ESP_LOGW(TAG, "cmd %d: toggle deceleration fading to: %d", count, (int)decelEnabled);
            if (decelEnabled){
                buzzer->beep(3, 60, 50);
            } else {
                buzzer->beep(1, 1000, 1);
            }
            break;

        case 12:
            ESP_LOGW(TAG, "cmd %d: sending button event to control task", count);
            //-> toggle altStickMapping (executed in control task)
            control->sendButtonEvent(count); //TODO: always send button event to control task (not just at count=1)?
            break;

    }
}




//-----------------------------
//------ startHandleLoop ------
//-----------------------------
//this function has to be started once in a separate task
//repeatedly evaluates and processes button events then takes the corresponding action
void buttonCommands::startHandleLoop() {

    while(1) {
        vTaskDelay(20 / portTICK_PERIOD_MS);
        //run handle function of evaluatedSwitch object
        button->handle();

        //--- count button presses and run action ---
        switch(state) {
            case inputState_t::IDLE: //wait for initial button press
                if (button->risingEdge) {
                    count = 1;
                    buzzer->beep(1, 65, 0);
                    timestamp_lastAction = esp_log_timestamp();
                    state = inputState_t::WAIT_FOR_INPUT;
                    ESP_LOGI(TAG, "first button press detected -> waiting for further events");
                }
                break;

            case inputState_t::WAIT_FOR_INPUT: //wait for further presses
                //button pressed again
                if (button->risingEdge){
                    count++;
                    buzzer->beep(1, 65, 0);
                    timestamp_lastAction = esp_log_timestamp();
                    ESP_LOGI(TAG, "another press detected -> count=%d -> waiting for further events", count);
                }
                //timeout
                else if (esp_log_timestamp() - timestamp_lastAction > 1000) {
                    state = inputState_t::IDLE;
                    buzzer->beep(count, 50, 50);
                    //TODO: add optional "bool wait" parameter to beep function to delay until finished beeping
                    ESP_LOGI(TAG, "timeout - running action function for count=%d", count);
                    //--- run action function ---
                    //check if still pressed
                    bool lastPressLong = false;
                    if (button->state == true){
                        //run special case when last press was longer than timeout
                        lastPressLong = true;
                    }
                        //run action function with current count of button presses
                        action(count, lastPressLong);
                    }
                break;
        }
    }
}




