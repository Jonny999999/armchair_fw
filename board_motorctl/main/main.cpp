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
#include "driver/uart.h"

	//custom C files
#include "wifi.h"
}

//=========================
//======= UART TEST =======
//=========================
//only run uart test code at the end
//disables other functionality
#define UART_TEST_ONLY


//tag for logging
static const char * TAG = "main";

#ifndef UART_TEST_ONLY
//custom C++ files
#include "config.hpp"
#include "control.hpp" 



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
	//esp_log_level_set("motordriver", ESP_LOG_INFO);
	//esp_log_level_set("motor-control", ESP_LOG_DEBUG);
	//esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
	//esp_log_level_set("joystickCommands", ESP_LOG_DEBUG);
	esp_log_level_set("button", ESP_LOG_INFO);
	esp_log_level_set("control", ESP_LOG_INFO);
	esp_log_level_set("fan-control", ESP_LOG_INFO);
	esp_log_level_set("wifi", ESP_LOG_INFO);
	esp_log_level_set("http", ESP_LOG_INFO);
	esp_log_level_set("automatedArmchair", ESP_LOG_DEBUG);
	//esp_log_level_set("current-sensors", ESP_LOG_INFO);
}
#endif



#ifdef UART_TEST_ONLY
void uart_init(void){
	uart_config_t uart1_config = {                                             
		.baud_rate = 115198,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_EVEN,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	ESP_LOGW(TAG, "config...");
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart1_config));                 
	ESP_LOGW(TAG, "setpins...");                                               
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 23, 22, 0, 0));                       
	ESP_LOGW(TAG, "init...");                                                  
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 10, NULL, 0));  

}

//struct for testing uart
typedef struct {
	uint32_t timestamp;
	int id;
	float value;
} uartDataStruct;
#endif

//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {
#ifndef UART_TEST_ONLY
	//enable 5V volate regulator
	gpio_pad_select_gpio(GPIO_NUM_17);                                                  
	gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_17, 1);                                                      

	//---- define log levels ----
	setLoglevels();

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


	//--- main loop ---
	//does nothing except for testing things
	while(1){
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

#endif



#ifdef UART_TEST_ONLY
	uart_init();
	uint8_t receivedData[sizeof(uartDataStruct)];
	uartDataStruct data;

	ESP_LOGW(TAG, "startloop...");
	while(1){
		vTaskDelay(500 / portTICK_PERIOD_MS);
		int len = uart_read_bytes(UART_NUM_1, receivedData, sizeof(uartDataStruct), 20 / portTICK_PERIOD_MS);
		uart_flush_input(UART_NUM_1);
		if (len > 0){
			memcpy(&data, receivedData, sizeof(uartDataStruct));
			//uart_write_bytes(UART_NUM_1, (const char *) data, 1);
			//ESP_LOGW(TAG, "sent data back %d", *data);
			ESP_LOGW(TAG, "received len=%d DATA: timestamp=%d, id=%d, value=%.1f", len, data.timestamp, data.id, data.value);
		}
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
