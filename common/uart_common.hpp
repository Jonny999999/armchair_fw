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
#include "freertos/semphr.h"

#include "freertos/queue.h"
#include "driver/uart.h"
}
#include "types.hpp"

//idea with time gap dropped, replaced with encoding
//#define UART_RECEIVE_DURATION 10 
//receive interval may not miss messages but also may not read multiple messages
//has to be significantly smaller than WRITE_MNIN_DELAY but larger than longest message
//semaphore for assuring min delay between sent data
//extern SemaphoreHandle_t uart_semaphoreSend;

//==== parameters message frame ====
#define START_PATTERN 0xAA
#define END_PATTERN 0xBB
#define MAX_MESSAGE_LENGTH 1024
#define ESCAPE_BYTE   0xCC
//min delay after each message (no significant effect)
#define UART_WRITE_MIN_DELAY 50
#define UART_WRITE_WAIT_TIMEOUT 2000

extern bool uart_isInitialized;

//struct for testing uart
typedef struct {
	uint32_t timestamp;
	int id;
	float value;
} uartData_test_t;


//unnecessary, using commands struct directly
typedef struct {
	uint32_t timestamp;
	motorCommands_t commands;
} uartData_motorCommands_t;


//function that handles receieved messages as byte array
//-> do something with received data
typedef void (*messageHandleFunc_t)(uint8_t *data, int length);



//===== uart_init =====
//should be run once at startup
void uart_init(void);


//===== uart_processReceivedByte =====
//decode received message to byte array (start, stop, escape pattern)
//has to be run for each receibed byte
//runs provided messageHandler function when complete message received
void uart_processReceivedByte(uint8_t data, messageHandleFunc_t messageHandler);


//===== uart_sendBinary =====
//encode byte array to message (start, stop, escape pattern)
//and send it via uart
void uart_sendBinary(uint8_t *data, int length);



//============================
//====== uart_sendStruct =====
//============================
//send and struct via uart
//waits when another struct was sent less than UART_WRITE_MIN_DELAY ago
template <typename T> void uart_sendStruct(T dataStruct) {
	static const char * TAG = "uart-common";
	//check if uart is initialized
	if (!uart_isInitialized){
		ESP_LOGE(TAG, "uart not initialized! droping data");
		return;
	}
	uint8_t dataSerial[sizeof(T)];
	//struct to serial bytes
	memcpy(dataSerial, &dataStruct, sizeof(T));
	// 	//wait for min delay after last write DROPPED
	// 	if (xSemaphoreTake(uart_semaphoreSend, UART_WRITE_WAIT_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE){
	// 		//send bytes
	// 		uart_write_bytes(UART_NUM_1, (const char *)dataSerial, sizeof(T));
	// 		ESP_LOGW(TAG, "sent data struct with len %d", sizeof(T));
	// 	} else ESP_LOGE(TAG, "timeout waiting for uart write semaphore! dropping data");
	// 	//wait min delay before next write is allowed
	// 	vTaskDelay(UART_WRITE_MIN_DELAY / portTICK_PERIOD_MS);
	// 	xSemaphoreGive(uart_semaphoreSend);

	uart_sendBinary(dataSerial, sizeof(T));
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
	ESP_LOGI(TAG, "converted serial data len=%d to struct", sizeof(T));
	return dataStruct;
}

