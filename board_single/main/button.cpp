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
    buttonCommands commandButton(objects->control, objects->joystick, objects->encoderQueue, objects->motorLeft, objects->motorRight, objects->legRest, objects->backRest, objects->buzzer);
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
    controlledMotor * motorLeft_f,
    controlledMotor *motorRight_f,
    cControlledRest *legRest_f,
    cControlledRest *backRest_f,
    buzzer_t *buzzer_f)
{
    // copy object pointers
    control = control_f;
    joystick = joystick_f;
    encoderQueue = encoderQueue_f;
    motorLeft = motorLeft_f;
    motorRight = motorRight_f;
    buzzer = buzzer_f;
    legRest = legRest_f;
    backRest = backRest_f;
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

        case 7:
            legRest->setTargetPercent(100);
            backRest->setTargetPercent(0);
            ESP_LOGW(TAG, "7x TESTING: set leg/back rest to 100/0");
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
#define INPUT_TIMEOUT 600 // duration of no button events, after which action is run (implicitly also is 'long-press' time)
#define IGNORE_BUTTON_TIME_SINCE_LAST_ROTATE 800 // time that has to be passed since last encoder rotate click for button count command to be accepted (e.g. prevent long press action after PRESS+ROTATE was used)
#define IGNORE_ROTATE_COUNT 1 //amount of ignored clicks before action is actually taken (ignore accidental touches)
void buttonCommands::startHandleLoop()
{
    //-- variables --
    static bool isPressed = false;
    static rotary_encoder_event_t event; // store event data
    int rotateCount = 0; // temporary count clicks encoder was rotated
    uint32_t timestampLastRotate = 0;
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
            case RE_ET_CHANGED:
                // ignore first clicks
                if (rotateCount++ < IGNORE_ROTATE_COUNT)
                    {
                        buzzer->beep(1, 20, 0);
                        break;
                    }
                timestampLastRotate = esp_log_timestamp();
                if (isPressed){
                    /////### scroll through status pages when PRESSED + ROTATED ###
                    ///if (event.diff > 0)
                    ///    display_rotateStatusPage(true, true); // select NEXT status screen, stay at last element (dont rotate to first)
                    ///else
                    ///    display_rotateStatusPage(false, true); // select PREVIOUS status screen, stay at first element (dont rotate to last)
                //### adjust back support when PRESSED + ROTATED ###
                    if (event.diff > 0)
                        backRest->setTargetPercent(backRest->getTargetPercent() - 5);
                    else
                        backRest->setTargetPercent(backRest->getTargetPercent() + 5);
                    // show temporary notification on display
                    char buf[8];
                    snprintf(buf, 8, "%.0f%%", backRest->getTargetPercent());
                    display_showNotification(2500, "moving Rest:", "BACK", buf);
                }
                //### adjust leg support when ROTATED ###
                else
                {
                    // increment target position each click
                    if (event.diff > 0)
                        legRest->setTargetPercent(legRest->getTargetPercent() - 10);
                    else
                        legRest->setTargetPercent(legRest->getTargetPercent() + 10);
                    // show temporary notification on display
                    char buf[8];
                    snprintf(buf, 8, "%.0f%%", legRest->getTargetPercent());
                    display_showNotification(2500, "moving Rest:", "LEG", buf);
                }
                buzzer->beep(1, 40, 0);
                break;
            case RE_ET_BTN_LONG_PRESSED:
            case RE_ET_BTN_CLICKED:
            default:
                break;
            }
        }
        else // timeout (no event received within TIMEOUT)
        {
            rotateCount = 0; // reset rotate count
            // ignore button click events when "ROTATE+PRESSED" was just used
            if (count > 0 && (esp_log_timestamp() - timestampLastRotate < IGNORE_BUTTON_TIME_SINCE_LAST_ROTATE))
                ESP_LOGW(TAG, "ignoring button count %d because encoder was rotated less than %d ms ago", count, IGNORE_BUTTON_TIME_SINCE_LAST_ROTATE);
            // encoder was pressed
            else if (count > 0)
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
