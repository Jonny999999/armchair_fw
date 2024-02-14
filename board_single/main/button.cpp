extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
}

#include "button.hpp"
#include "encoder.hpp"



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
    //--- variables ---
    bool decelEnabled; //for different beeping when toggling
    commandSimple_t cmds[8]; //array for commands for automatedArmchair

    //--- get joystick position ---
    //in case joystick is used for additional cases:
    //joystickData_t stickData = joystick->getData();

    //--- run actions based on count ---
    switch (count)
    {
    // ## no command ##
    default:
        ESP_LOGE(TAG, "no command for count=%d and long=%d defined", count, lastPressLong);
        buzzer->beep(3, 400, 100);
        break;

    case 1:
        // ## switch to MENU state ##
        if (lastPressLong)
        {
            control->changeMode(controlMode_t::MENU);
            ESP_LOGW(TAG, "1x long press -> change to menu mode");
            buzzer->beep(1, 1000, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        // ## toggle joystick freeze ##
        else if (control->getCurrentMode() == controlMode_t::MASSAGE)
        {
            control->toggleFreezeInputMassage();
        }
        // ## define joystick center ##
        else
        {
            // note: disabled joystick calibration due to accidential trigger
            //joystick->defineCenter();
        }
        break;

    case 2:
        // ## switch to ADJUST_CHAIR mode ##
        if (lastPressLong)
        {
            ESP_LOGW(TAG, "cmd %d: toggle ADJUST_CHAIR", count);
            control->toggleMode(controlMode_t::ADJUST_CHAIR);
            }
            // ## toggle IDLE ##
            else {
            ESP_LOGW(TAG, "cmd %d: toggle IDLE", count);
            control->toggleIdle(); //toggle between idle and previous/default mode
            }
            break;

        case 3:
        // ## switch to JOYSTICK mode ##
            ESP_LOGW(TAG, "cmd %d: switch to JOYSTICK", count);
            control->changeMode(controlMode_t::JOYSTICK); //switch to JOYSTICK mode
            break;

        case 4:
        // ## switch to HTTP mode ##
            ESP_LOGW(TAG, "cmd %d: toggle between HTTP and JOYSTICK", count);
            control->toggleModes(controlMode_t::HTTP, controlMode_t::JOYSTICK); //toggle between HTTP and JOYSTICK mode
            break;

        case 6:
        // ## switch to MASSAGE mode ##
            ESP_LOGW(TAG, "cmd %d: toggle between MASSAGE and JOYSTICK", count);
            control->toggleModes(controlMode_t::MASSAGE, controlMode_t::JOYSTICK); //toggle between MASSAGE and JOYSTICK mode
            break;

        case 8:
        // ## toggle "sport-mode" ##
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
        // ## toggle alternative stick mapping ##
            control->toggleAltStickMapping();
            break;
        }
}




//-----------------------------
//------ startHandleLoop ------
//-----------------------------
// when not in MENU mode, repeatedly receives events from encoder button
// and takes the corresponding action
// this function has to be started once in a separate task
#define INPUT_TIMEOUT 800 // duration of no button events, after which action is run (implicitly also is 'long-press' time)
void buttonCommands::startHandleLoop()
{
    //-- variables --
    bool isPressed = false;
    static rotary_encoder_event_t ev; // store event data
    // int count = 0; (from class)

    while (1)
    {
        //-- disable functionality when in menu mode --
        //(display task uses encoder in that mode)
        if (control->getCurrentMode() == controlMode_t::MENU)
        {
            //do nothing every loop cycle
            ESP_LOGD(TAG, "in MENU mode -> button commands disabled");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        //-- get events from encoder --
        if (xQueueReceive(encoderQueue, &ev, INPUT_TIMEOUT / portTICK_PERIOD_MS))
        {
            control->resetTimeout(); //reset inactivity IDLE timeout
            switch (ev.type)
            {
                break;
            case RE_ET_BTN_PRESSED:
                ESP_LOGD(TAG, "Button pressed");
                buzzer->beep(1, 65, 0);
                isPressed = true;
                count++; // count each pressed event
                break;
            case RE_ET_BTN_RELEASED:
                ESP_LOGD(TAG, "Button released");
                isPressed = false; // rest stored state
                break;
            case RE_ET_BTN_LONG_PRESSED:
            case RE_ET_BTN_CLICKED:
            case RE_ET_CHANGED:
            default:
                break;
            }
        }
        else // timeout (no event received within TIMEOUT)
        {
            if (count > 0)
            {
                //-- run action with count of presses --
                ESP_LOGI(TAG, "timeout: count=%d, lastPressLong=%d -> running action", count, isPressed);
                buzzer->beep(count, 50, 50);
                action(count, isPressed); // run action - if currently still on the last press is considered long
                count = 0;                // reset count
            }
            else {
                ESP_LOGD(TAG, "no button event received in this cycle (count=0)");
            }
        } //end queue
    } //end while(1)
} //end function