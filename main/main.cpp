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


extern "C" void app_main(void) {

    //set loglevel for all tags:
    esp_log_level_set("*", ESP_LOG_INFO);
    //set loglevel for motordriver to DEBUG for testing
    esp_log_level_set("motordriver", ESP_LOG_DEBUG);

    //configure motor driver
    single100a_config_t configDriverLeft = {
        .gpio_pwm = GPIO_NUM_14,
        .gpio_a = GPIO_NUM_12,
        .gpio_b = GPIO_NUM_13,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .abInverted = true,
        .resolution = LEDC_TIMER_11_BIT,
        .pwmFreq = 10000
    };

    //create driver instance for motor left
    single100a motorLeft(configDriverLeft);


    while(1){

        //--- testing the motor driver ---
        //fade up duty - forward
        for (int duty=0; duty<=100; duty+=5) {
            motorLeft.set(motorstate_t::FWD, duty);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        //brake for 1 s
        motorLeft.set(motorstate_t::BRAKE);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //stay at 100% - reverse
        motorLeft.set(motorstate_t::REV, 150);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        //stay at idle (default duty)
        motorLeft.set(motorstate_t::IDLE);
        vTaskDelay(2000 / portTICK_PERIOD_MS);

    }
}
