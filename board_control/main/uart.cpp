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



void uart_task_testing(void *arg){
	//repeatedly send 8 bit count and log received 1 byte
	uint8_t *data = (uint8_t *) malloc(1024);
	uint8_t count = 0;
	ESP_LOGW(TAG, "startloop...");
	while (1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
		int len = uart_read_bytes(UART_NUM_1, data, (1024 - 1), 20 / portTICK_PERIOD_MS);
		//uart_flush_input(UART_NUM_1);
		//uart_flush(UART_NUM_1);
		ESP_LOGW(TAG, "received len=%d data=%d", len, *data);
		*data = 99; //set to 99 (indicates no new data received)
		uart_write_bytes(UART_NUM_1, (const char *) &count, 1);
		ESP_LOGW(TAG, "sent data %d", count);
		count++;
	}
}



void task_uartReceive(void *arg){
	static const char * TAG = "uart-receive";
	//repeatedly send 8 bit count and log received 1 byte
	char *data = (char *) malloc(1024);
	char count = 0;
	ESP_LOGW(TAG, "startloop...");
	while (1) {
		vTaskDelay(200 / portTICK_PERIOD_MS);
		int len = uart_read_bytes(UART_NUM_1, data, (1024 - 1), 20 / portTICK_PERIOD_MS);
		if (len>0) ESP_LOGW(TAG, "received len=%d data=%d", len, *data);
	}
}



void task_uartReceiveQueue(void *arg){
	static const char * TAG = "uart-receive";
	while (1) {
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}
}




//  //send incrementing count
//  void task_uartSend(void *arg){
//  	static const char * TAG = "uart-send";
//  	//repeatedly send 8 bit count and log received 1 byte
//  	char *data = (char *) malloc(1024);
//  	char count = 0;
//  	ESP_LOGW(TAG, "startloop...");
//  	while (1) {
//  		vTaskDelay(200 / portTICK_PERIOD_MS);    
//  		uart_write_bytes(UART_NUM_1, (const char *) &count, 1);                    
//  		ESP_LOGW(TAG, "sent data %d", (int)count);                              
//  		count++;
//  	}
//  	ESP_LOGE(TAG, "loop exit...");
//  }



//send struct
void task_uartSend(void *arg){
	static const char * TAG = "uart-send";
	uartDataStruct data = {123, 0, 1.1};
	uint8_t serialData[sizeof(uartDataStruct)];
	char count = 0;
	ESP_LOGW(TAG, "startloop...");
	while (1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);    
		memcpy(serialData, &data, sizeof(uartDataStruct));
		uart_write_bytes(UART_NUM_1, (const char *)serialData, sizeof(uartDataStruct));
		ESP_LOGW(TAG, "sent data struct with len %d", sizeof(uartDataStruct));
		ESP_LOGW(TAG, "sent DATA: timestamp=%d, id=%d, value=%.1f", data.timestamp, data.id, data.value);

		//change data values
		data.timestamp = esp_log_timestamp();
		data.id++;
		data.value += 0.6;
	}
	ESP_LOGE(TAG, "loop exit...");
}
