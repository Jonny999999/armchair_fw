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
#include "display.hpp"

// tag for logging
static const char *TAG = "button";

//======================================
//============ button task =============
//======================================
// task that handles the button interface/commands
void task_button(void *task_button_parameters)
{
    task_button_parameters_t *objects = (task_button_parameters_t *)task_button_parameters;
    ESP_LOGI(TAG, "Initializing command-button and starting handle loop");
    // create button instance
    buttonCommands commandButton(objects->control, objects->joystick, objects->encoderQueue, objects->motorLeft, objects->motorRight, objects->buzzer);
    // start handle loop
    commandButton.startHandleLoop();
}

//-----------------------------
//-------- constructor --------
//-----------------------------
buttonCommands::buttonCommands(
    controlledArmchair *control_f,
    evaluatedJoystick *joystick_f,
    QueueHandle_t encoderQueue_f,
    controlledMotor *motorLeft_f,
    controlledMotor *motorRight_f,
    buzzer_t *buzzer_f)
{
    // copy object pointers
    control = control_f;
    joystick = joystick_f;
    encoderQueue = encoderQueue_f;
    motorLeft = motorLeft_f;
    motorRight = motorRight_f;
    buzzer = buzzer_f;
    // TODO declare / configure evaluatedSwitch here instead of config (unnecessary that button object is globally available - only used here)?
}

//----------------------------
//--------- action -----------
//----------------------------
//function that runs commands depending on a count value
void buttonCommands::action (uint8_t count, bool lastPressLong){
    //--- variables ---
    bool decelEnabled; //for different beeping when toggling
    commandSimple_t cmds[8]; //array for commands for automatedArmchair_c

    //--- get joystick position ---
    //in case joystick is used for additional cases:
    //joystickData_t stickData = joystick->getData();

    //--- run actions based on count ---
    switch (count)
    {
    // ## no command ##
    default:
        ESP_LOGE(TAG, "no command for count=%d and long=%d defined", count, lastPressLong);
        buzzer->beep(3, 200, 100);
        break;

    case 1:
        // ## switch to MENU_SETTINGS state ##
        if (lastPressLong)
        {
            ESP_LOGW(TAG, "1x long press -> clear encoder queue and change to mode 'menu mode select'");
            buzzer->beep(5, 50, 30);
            // clear encoder event queue (prevent menu from exiting immediately due to long press event just happend)
            vTaskDelay(200 / portTICK_PERIOD_MS);
            //TODO move encoder queue clear to changeMode() method?
            rotary_encoder_event_t ev;
            while (xQueueReceive(encoderQueue, &ev, 0) == pdPASS);
            control->changeMode(controlMode_t::MENU_MODE_SELECT);
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
            ESP_LOGW(TAG, "cmd %d: switch to ADJUST_CHAIR", count);
            control->changeMode(controlMode_t::ADJUST_CHAIR);
        }
        // ## toggle IDLE ##
        else
        {
            ESP_LOGW(TAG, "cmd %d: toggle IDLE", count);
            control->toggleIdle(); // toggle between idle and previous/default mode
        }
        break;

        case 3:
        // ## switch to JOYSTICK mode ##
            ESP_LOGW(TAG, "cmd %d: switch to JOYSTICK", count);
            control->changeMode(controlMode_t::JOYSTICK); //switch to JOYSTICK mode
            break;

        case 4:
        // ## switch to HTTP mode ##
            ESP_LOGW(TAG, "cmd %d: switch to HTTP", count);
            control->changeMode(controlMode_t::HTTP); //switch to HTTP mode
            break;
        
        case 5:
        // ## switch to MENU_SETTINGS state ##
            ESP_LOGW(TAG, "5x press -> clear encoder queue and change to mode 'menu settings'");
            buzzer->beep(20, 20, 10);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            // clear encoder event queue (prevent menu from using previous events)
            rotary_encoder_event_t ev;
            while (xQueueReceive(encoderQueue, &ev, 0) == pdPASS);
            control->changeMode(controlMode_t::MENU_SETTINGS);
            break;

        case 6:
        // ## switch to MASSAGE mode ##
            ESP_LOGW(TAG, "switch to MASSAGE");
            control->changeMode(controlMode_t::MASSAGE); //switch to MASSAGE mode
            break;

        case 8:
        // ## toggle "sport-mode" ##
            //toggle deceleration fading between on and off
            //decelEnabled = motorLeft->toggleFade(fadeType_t::DECEL);
            //motorRight->toggleFade(fadeType_t::DECEL);
            decelEnabled = motorLeft->toggleFade(fadeType_t::ACCEL); //TODO remove/simplify this using less functions
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
// when not in MENU_SETTINGS mode, repeatedly receives events from encoder button
// and takes the corresponding action
// this function has to be started once in a separate task
#define INPUT_TIMEOUT 500 // duration of no button events, after which action is run (implicitly also is 'long-press' time)
void buttonCommands::startHandleLoop()
{
    //-- variables --
    bool isPressed = false;
    static rotary_encoder_event_t event; // store event data
    // int count = 0; (from class)

    while (1)
    {
        //-- disable functionality when in menu mode --
        //(display task uses encoder in that mode)
        if (control->getCurrentMode() == controlMode_t::MENU_SETTINGS 
        || control->getCurrentMode() == controlMode_t::MENU_MODE_SELECT)
        {
            //do nothing every loop cycle
            ESP_LOGD(TAG, "in MENU_SETTINGS or MENU_MODE_SELECT mode -> button commands disabled");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        //-- get events from encoder --
        if (xQueueReceive(encoderQueue, &event, INPUT_TIMEOUT / portTICK_PERIOD_MS))
        {
            control->resetTimeout();          // user input -> reset switch to IDLE timeout
            switch (event.type)
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
            case RE_ET_CHANGED: // scroll through status pages when simply rotating encoder
                if (event.diff > 0)
                {
                    display_rotateStatusPage(true, true); //select NEXT status screen, stau at last element (dont rotate to first)
                    buzzer->beep(1, 65, 0);
                }
                else
                {
                    display_rotateStatusPage(false, true); //select PREVIOUS status screen, stay at first element (dont rotate to last)
                    buzzer->beep(1, 65, 0);
                }
                break;
            case RE_ET_BTN_LONG_PRESSED:
            case RE_ET_BTN_CLICKED:
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
                buzzer->beep(count, 50, 50, 200); //beep count, with 200ms gap before next queued beeps can start
                action(count, isPressed); // run action - if currently still on the last press is considered long
                count = 0;                // reset count
            }
            else {
                ESP_LOGD(TAG, "no button event received in this cycle (count=0)");
            }
        } //end queue
    } //end while(1)
} //end function
