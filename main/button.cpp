extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
}

#include "control.hpp"
#include "button.hpp"



//tag for logging
static const char * TAG = "button";



//-----------------------------
//-------- constructor --------
//-----------------------------
buttonCommands::buttonCommands(gpio_evaluatedSwitch * button_f, buzzer_t * buzzer_f ){
    //copy object pointers
    button = button_f;
    buzzer = buzzer_f;
    //TODO declare / configure evaluatedSwitch here instead of config (unnecessary that button object is globally available - only used here)?
}



//----------------------------
//--------- action -----------
//----------------------------
//function that runs commands depending on a count value
void buttonCommands::action (uint8_t count){
    switch (count){
        //no such command
        default:
            ESP_LOGE(TAG, "no command for count=%d defined", count);
            buzzer->beep(3, 400, 100);
            break;

        case 1:
            ESP_LOGW(TAG, "running command for count 1");
            buzzer->beep(1,500,1);
            break;

        case 2:
            ESP_LOGW(TAG, "cmd %d: switching to IDLE", count);
            control_changeMode(controlMode_t::IDLE);
            buzzer->beep(1,1000,1);
            break;

        case 3:
            ESP_LOGW(TAG, "cmd %d: switching to JOYSTICK", count);
            control_changeMode(controlMode_t::JOYSTICK);
            buzzer->beep(2,400,100);
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
                    buzzer->beep(1, 60, 0);
                    timestamp_lastAction = esp_log_timestamp();
                    state = inputState_t::WAIT_FOR_INPUT;
                    ESP_LOGI(TAG, "first button press detected -> waiting for further events");
                }
                break;

            case inputState_t::WAIT_FOR_INPUT: //wait for further presses
                //button pressed again
                if (button->risingEdge){
                    count++;
                    buzzer->beep(1, 60, 0);
                    timestamp_lastAction = esp_log_timestamp();
                    ESP_LOGI(TAG, "another press detected -> count=%d -> waiting for further events", count);
                }
                //timeout
                else if (esp_log_timestamp() - timestamp_lastAction > 1000) {
                    state = inputState_t::IDLE;
                    buzzer->beep(count, 50, 50);
                    //TODO: add optional "bool wait" parameter to beep function to delay until finished beeping
                    //run action function with current count of button presses
                    ESP_LOGI(TAG, "timeout - running action function for count=%d", count);
                    action(count);
                }
                break;
        }
    }
}




