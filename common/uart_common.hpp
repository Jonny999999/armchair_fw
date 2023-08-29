#pragma once
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

//struct for testin uart
typedef struct {
	uint32_t timestamp;
	int id;
	float value;
} uartData_test_t;


//===== uart_init =====
//should be run once at startup
void uart_init(void);


//============================
//====== uart_sendStruct =====
//============================
//send and struct via uart
template <typename T> void uart_sendStruct(T dataStruct) {
	static const char * TAG = "uart-common";
	//TODO check if initialized?
	uint8_t dataSerial[sizeof(T)];
	memcpy(dataSerial, &dataStruct, sizeof(T));
	uart_write_bytes(UART_NUM_1, (const char *)dataSerial, sizeof(T));
	ESP_LOGW(TAG, "sent data struct with len %d", sizeof(T));
	//ESP_LOGW(TAG, "sent DATA: timestamp=%d, id=%d, value=%.1f", data.timestamp, data.id, data.value);
}



//==============================
//====== serialData2Struct =====
//==============================
//convert serial data (byte array) to given struct and return it
//note check whether serial data length actually matches size of struct is necessary before
template <typename T> T serialData2Struct(uint8_t *dataSerial){
	static const char * TAG = "uart-common";
	T dataStruct;
	memcpy(&dataStruct, dataSerial, sizeof(T));
	ESP_LOGW(TAG, "converted serial data len=%d to struct", sizeof(T));
	return dataStruct;
}

