#include "uart.hpp"
//===== uart board MOTORCTL =====

static const char * TAG = "uart";


//==============================
//====== task_uartReceive ======
//==============================
void task_uartReceive(void *arg){
	//receive data from uart, detect associated struct and copy/handle the data
	//TODO use queue instead of check interval?
	uartData_test_t testData;
	uint8_t receivedData[1024-1];
	while(1){
		//note: check has to be more frequent than pause time between sending
		vTaskDelay(200 / portTICK_PERIOD_MS);
		int len = uart_read_bytes(UART_NUM_1, receivedData, sizeof(uartData_test_t), 20 / portTICK_PERIOD_MS);
		uart_flush_input(UART_NUM_1);
		if (len < 1) continue;
		switch (len){
			case sizeof(uartData_test_t):
				testData = serialData2Struct<uartData_test_t>(receivedData);
				ESP_LOGW(TAG, "received uartDataStruct len=%d DATA: timestamp=%d, id=%d, value=%.1f", len, testData.timestamp, testData.id, testData.value);
				break;
				//TODO add other received structs here
			default:
				ESP_LOGW(TAG, "received data len=%d cant be associated with configures struct", len);
				break;
		}
	}
}



//=============================
//======= task_uartSend =======
//=============================
//TODO copy send task from board_control/uart.cpp
void task_uartSend(void *arg){
	while (1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);    
	}
}
