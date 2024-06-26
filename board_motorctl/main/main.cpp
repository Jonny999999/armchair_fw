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
#include "esp_spiffs.h"    
#include <string.h>

#include "driver/ledc.h"

	//custom C files
#include "wifi.h"
}
//custom C++ files
#include "config.hpp"
#include "uart.hpp"
#include "speedsensor.hpp"


//============================
//======= TESTING MODE =======
//============================
//do not start the actual tasks for controlling the armchair
#define TESTING_MODE

//=========================
//======= UART TEST =======
//=========================
//only run uart test code at the end
//disables other functionality
//#define UART_TEST_ONLY

//==========================
//======= BRAKE TEST =======
//==========================
//only run brake-test (ignore uart input)
//#define BRAKE_TEST_ONLY

//====================-======
//==== SPEED SENSOR TEST ====
//======================-====
//only run speed-sensor test
#define SPEED_SENSOR_TEST

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
		vTaskDelay(10 / portTICK_PERIOD_MS);
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



//=======================================
//============== fan task ===============
//=======================================
//task that controlls fans for cooling the drivers
void task_fans( void * pvParameters ){
	ESP_LOGI(TAG, "Initializing fans and starting fan handle loop");
	//create fan instances with config defined in config.cpp
	controlledFan fan(configCooling, &motorLeft, &motorRight);
	//repeatedly run fan handle function in a slow loop
	while(1){
		fan.handle();
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}



//==================================
//======== define loglevels ========
//==================================
void setLoglevels(void){
	//set loglevel for all tags:
	esp_log_level_set("*", ESP_LOG_WARN);

	//--- set loglevel for individual tags ---
	esp_log_level_set("main", ESP_LOG_INFO);
	esp_log_level_set("buzzer", ESP_LOG_ERROR);
	esp_log_level_set("motordriver", ESP_LOG_VERBOSE);
	esp_log_level_set("motor-control", ESP_LOG_INFO);
	//esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
	//esp_log_level_set("joystickCommands", ESP_LOG_DEBUG);
	esp_log_level_set("button", ESP_LOG_INFO);
	esp_log_level_set("control", ESP_LOG_INFO);
	esp_log_level_set("fan-control", ESP_LOG_INFO);
	esp_log_level_set("wifi", ESP_LOG_INFO);
	esp_log_level_set("http", ESP_LOG_INFO);
	esp_log_level_set("automatedArmchair", ESP_LOG_DEBUG);
	esp_log_level_set("uart_common", ESP_LOG_INFO);
	esp_log_level_set("uart", ESP_LOG_INFO);
	//esp_log_level_set("current-sensors", ESP_LOG_INFO);
	esp_log_level_set("speedSensor", ESP_LOG_WARN);
}



//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {

	//---- define log levels ----
	setLoglevels();

#ifndef TESTING_MODE
	//enable 5V volate regulator
	gpio_pad_select_gpio(GPIO_NUM_17);                                                  
	gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_17, 1);                                                      


	//----------------------------------------------
	//--- create task for controlling the motors ---
	//----------------------------------------------
	//task that receives commands, handles ramp and current limit and executes commands using the motordriver function
	xTaskCreate(&task_motorctl, "task_motor-control", 2048, NULL, 6, NULL);

	//------------------------------
	//--- create task for buzzer ---
	//------------------------------
	xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 2, NULL);


	//-----------------------------------
	//--- create task for fan control ---
	//-----------------------------------
	//task that evaluates and processes the button input and runs the configured commands
	xTaskCreate(&task_fans, "task_fans", 2048, NULL, 1, NULL);


	//beep at startup
	buzzer.beep(3, 70, 50);
#endif



#ifndef BRAKE_TEST_ONLY
	//-------------------------------------------
	//--- create tasks for uart communication ---
	//-------------------------------------------
	uart_init();
	xTaskCreate(task_uartReceive, "task_uartReceive", 4096, NULL, 10, NULL);
	xTaskCreate(task_uartSend, "task_uartSend", 4096, NULL, 10, NULL);
#endif



#ifdef SPEED_SENSOR_TEST
	speedSensor_config_t speedRight_config{
		.gpioPin = GPIO_NUM_18,
			.degreePerGroup = 72,
			.tireCircumferenceMeter = 0.69,
			.directionInverted = false,
			.logName = "speedRight",
	};
	speedSensor speedRight (speedRight_config);
#endif


	//---------------------------
	//-------- main loop --------
	//---------------------------
	//does nothing except for testing things
	while(1){

#ifdef SPEED_SENSOR_TEST
		vTaskDelay(100 / portTICK_PERIOD_MS);
		//speedRight.getRpm();
		ESP_LOGI(TAG, "speedsensor-test: rpm=%fRPM, speed=%fkm/h dir=%d, pulseCount=%d, p1=%d, p2=%d, p3=%d lastEdgetime=%d", speedRight.getRpm(), speedRight.getKmph(), speedRight.direction, speedRight.pulseCounter, (int)speedRight.pulseDurations[0]/1000,  (int)speedRight.pulseDurations[1]/1000, (int)speedRight.pulseDurations[2]/1000,(int)speedRight.lastEdgeTime);
#endif


#ifdef BRAKE_TEST_ONLY
		//test relays at standstill
		ESP_LOGW("brake-test", "test relays via motorctl");
		//turn on
		motorRight.setTarget(motorstate_t::BRAKE, 0);
		vTaskDelay(500 / portTICK_PERIOD_MS);
		motorRight.setTarget(motorstate_t::BRAKE, 0);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//turn off
		motorRight.setTarget(motorstate_t::IDLE, 0);
		vTaskDelay(500 / portTICK_PERIOD_MS);
		motorRight.setTarget(motorstate_t::IDLE, 0);

		vTaskDelay(1000 / portTICK_PERIOD_MS);

		//go forward and brake
		ESP_LOGW("brake-test", "go forward 30%% then brake");
		motorRight.setTarget(motorstate_t::FWD, 30);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		motorRight.setTarget(motorstate_t::BRAKE, 0);

		vTaskDelay(3000 / portTICK_PERIOD_MS);

		//brake partial
		ESP_LOGW("brake-test", "go forward 30%% then brake partial 10%%, hold for 5sec");
		motorRight.setTarget(motorstate_t::FWD, 30);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		motorRight.setTarget(motorstate_t::BRAKE, 10);

		vTaskDelay(5000 / portTICK_PERIOD_MS);
		//reset to idle
		motorRight.setTarget(motorstate_t::IDLE, 0);
#endif


		//--- test controlledMotor --- (ramp)
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
