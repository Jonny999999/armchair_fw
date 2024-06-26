#pragma once

#include <stdio.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
}

#include "freertos/queue.h"



//===================================
//========= buzzer_t class ==========
//===================================
//class which blinks a gpio pin for the provided count and durations.
//- 'processQueue' has to be run in a separate task
//- uses a queue to queue up multiple beep commands
class buzzer_t {
    public:
        //--- constructor ---
        buzzer_t(gpio_num_t gpio_pin_f, uint16_t msGap_f = 200);

        //--- functions ---
        void processQueue(); //has to be run once in a separate task, waits for and processes queued events
        //add entry to queue processing beeps, last parameter is optional to delay the next entry
        void beep(uint8_t count, uint16_t msOn, uint16_t msOff, uint16_t msDelayFinished);
        void beep(uint8_t count, uint16_t msOn, uint16_t msOff);
        //void clear(); (TODO - not implemented yet)
        //void createTask(); (TODO - not implemented yet)

        //--- variables ---
        
    private:
        //--- functions ---
        void init();

        //--- variables ---
        uint16_t msGap; //gap between beep entries (when multiple queued)
        gpio_num_t gpio_pin;

        struct beepEntry {
            uint8_t count;
            uint16_t msOn;
            uint16_t msOff;
            uint16_t msDelay;
        };

        //queue for queueing up multiple events while one is still processing
        QueueHandle_t beepQueue = NULL;

};


//======================================
//============ buzzer task =============
//======================================
// Task that repeatedly handles the buzzer object (process Queued beeps)
// Note: pointer to globally initialized buzzer object has to be passed as task-parameter
void task_buzzer(void * param_buzzerObject);