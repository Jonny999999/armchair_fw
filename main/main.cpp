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

#include "config.hpp"
#include "control.hpp" 
#include "button.hpp"

//tag for logging
static const char * TAG = "main";



//====================================
//========== motorctl task ===========
//====================================
//task for handling the motors (ramp, current limit, driver)
void task_motorctl( void * pvParameters ){
    ESP_LOGI(TAG, "starting handle loop...");
    while(1){
        motorRight.handle();
        motorLeft.handle();
        //10khz -> T=100us
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}



//======================================
//============ buzzer task =============
//======================================
//TODO: move the task creation to buzzer class (buzzer.cpp)
//e.g. only have function buzzer.createTask() in app_main
void task_buzzer( void * pvParameters ){
    ESP_LOGI("task_buzzer", "Start of buzzer task...");
        //run function that waits for a beep events to arrive in the queue
        //and processes them
        buzzer.processQueue();
}



//======================================
//============ button task =============
//======================================
void task_button( void * pvParameters ){
    ESP_LOGI(TAG, "Initializing command-button and starting handle loop");
    //create button instance
    buttonCommands commandButton(&buttonJoystick, &buzzer);
    //start handle loop
    commandButton.startHandleLoop();
}



//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {
    //enable 5V volate regulator
    gpio_pad_select_gpio(GPIO_NUM_17);                                                  
    gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_17, 1);                                                      



    //-------------------------------
    //---------- log level ----------
    //-------------------------------
    //set loglevel for all tags:
    esp_log_level_set("*", ESP_LOG_WARN);
    //set loglevel for individual tags:
    esp_log_level_set("main", ESP_LOG_INFO);
    //esp_log_level_set("motordriver", ESP_LOG_DEBUG);
    //esp_log_level_set("motor-control", ESP_LOG_DEBUG);
    //esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
    //esp_log_level_set("joystickCommands", ESP_LOG_DEBUG);
    esp_log_level_set("button", ESP_LOG_INFO);



    //----------------------------------------------
    //--- create task for controlling the motors ---
    //----------------------------------------------
    //task that receives commands, handles ramp and current limit and executes commands using the motordriver function
    xTaskCreate(&task_motorctl, "task_motor-control", 2048, NULL, 5, NULL);

    //------------------------------
    //--- create task for buzzer ---
    //------------------------------
    xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 5, NULL);

    //-------------------------------
    //--- create task for control ---
    //-------------------------------
    //task that generates motor commands depending on the current mode and sends those to motorctl task (task_control is defined in control.cpp)
    xTaskCreate(&task_control, "task_control", 2048, NULL, 5, NULL);

    //------------------------------
    //--- create task for button ---
    //------------------------------
    //task that evaluates and processes the button input and runs the configured commands
    xTaskCreate(&task_button, "task_buzzer", 2048, NULL, 5, NULL);


    //beep at startup
    buzzer.beep(3, 70, 50);


    while(1){

        vTaskDelay(500 / portTICK_PERIOD_MS);


       //--- testing button ---
       //buttonJoystick.handle();
       // if (buttonJoystick.risingEdge){
       //     ESP_LOGI(TAG, "button pressed, was released for %d ms", buttonJoystick.msReleased);
       //     buzzer.beep(2, 100, 50);

       // }else if (buttonJoystick.fallingEdge){
       //     ESP_LOGI(TAG, "button released, was pressed for %d ms", buttonJoystick.msPressed);
       //     buzzer.beep(1, 200, 0);
       // }



        //--- testing joystick commands ---
       // motorCommands_t commands = joystick_generateCommandsDriving(joystick);
       // motorRight.setTarget(commands.right.state, commands.right.duty); //TODO make motorctl.setTarget also accept motorcommand struct directly
       // motorLeft.setTarget(commands.left.state, commands.left.duty); //TODO make motorctl.setTarget also accept motorcommand struct directly
       // //motorRight.setTarget(commands.right.state, commands.right.duty);

        


        //--- testing joystick class ---
        //joystickData_t data = joystick.getData();
        //ESP_LOGI(TAG, "position=%s, x=%.1f%%, y=%.1f%%, radius=%.1f%%, angle=%.2f",
        //        joystickPosStr[(int)data.position], data.x*100, data.y*100, data.radius*100, data.angle);

        //--- testing the motor driver ---
        //fade up duty - forward
        //   for (int duty=0; duty<=100; duty+=5) {
        //       motorLeft.setTarget(motorstate_t::FWD, duty);
        //       vTaskDelay(100 / portTICK_PERIOD_MS); 
        //   }
        
        
       //--- testing controlledMotor --- (ramp)
       // //brake for 1 s
       // motorLeft.setTarget(motorstate_t::BRAKE);
       // vTaskDelay(1000 / portTICK_PERIOD_MS);
       // //command 90% - reverse
       // motorLeft.setTarget(motorstate_t::REV, 90);
       // vTaskDelay(5000 / portTICK_PERIOD_MS);
       // //command 100% - forward
       // motorLeft.setTarget(motorstate_t::FWD, 100);
       // vTaskDelay(1000 / portTICK_PERIOD_MS);

    }

}
