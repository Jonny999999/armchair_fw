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
#include <string.h>

#include "freertos/queue.h"
#include "driver/uart.h"
}

#include "uart.hpp"

static const char * TAG = "uart";


//TESTING
#include "control.hpp"
#include "config.hpp"

//==============================
//====== task_uartReceive ======
//==============================
//TODO copy receive task from board_motorctl/uart.cpp
void task_uartReceive(void *arg){
	//--- testing force http mode after startup ---
	//TESTING
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	control.changeMode(controlMode_t::HTTP);
	while (1) {
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}
}



//=============================
//======= task_uartSend =======
//=============================
//repeatedly send structs to uart
void task_uartSend(void *arg){
	static const char * TAG = "uart-send";
	uartData_test_t data = {123, 0, 1.1};
	ESP_LOGW(TAG, "startloop...");
	while (1) {
		vTaskDelay(10000 / portTICK_PERIOD_MS);    
		uart_sendStruct<uartData_test_t>(data);

		//change data values
		data.timestamp = esp_log_timestamp();
		data.id++;
		data.value += 0.6;
	}
	ESP_LOGE(TAG, "loop exit...");
}
