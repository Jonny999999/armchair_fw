extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "driver/ledc.h"

}

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "config.hpp"

//tag for logging
static const char * TAG = "main";



//====================================
//========== motorctl task ===========
//====================================
//task for handling the motors (ramp, current limit, driver)
void task_motorctl( void * pvParameters ){
    ESP_LOGI("motorctl-task", "starting handle loop...");
    while(1){
        motorLeft.handle();
        //10khz -> T=100us
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}



//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {

    //-------------------------------
    //---------- log level ----------
    //-------------------------------
    //set loglevel for all tags:
    esp_log_level_set("*", ESP_LOG_INFO);

    //set loglevel for individual tags:
    //esp_log_level_set("motordriver", ESP_LOG_DEBUG);
    esp_log_level_set("motor-control", ESP_LOG_DEBUG);



    //----------------------------------------------
    //--- create task for controlling the motors ---
    //----------------------------------------------
    xTaskCreate(&task_motorctl, "task_motor-control", 2048, NULL, 5, NULL);


    while(1){

        //--- testing the motor driver ---
        //fade up duty - forward
        //   for (int duty=0; duty<=100; duty+=5) {
        //       motorLeft.setTarget(motorstate_t::FWD, duty);
        //       vTaskDelay(100 / portTICK_PERIOD_MS);
        //   }
        //
        //--- testing controlledMotor --- (ramp)
        //brake for 1 s
        motorLeft.setTarget(motorstate_t::BRAKE);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //command 90% - reverse
        motorLeft.setTarget(motorstate_t::REV, 90);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        //command 100% - forward
        motorLeft.setTarget(motorstate_t::FWD, 100);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    }

}
