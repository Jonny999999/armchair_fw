#include "uart.hpp"
#include "config.hpp"
#include "types.hpp"
//===== uart board MOTORCTL =====

static const char * TAG = "uart";


//==============================
//====== task_uartReceive ======
//==============================
void task_uartReceive(void *arg){
	ESP_LOGW(TAG, "receive task started");
	//receive data from uart, detect associated struct and copy/handle the data
	//TODO use queue instead of check interval?
	uartData_test_t dataTest;
	motorCommands_t dataMotorCommands;
	uint8_t receivedData[1024-1];
	while(1){
		//note: check has to be more frequent than pause time between sending
		vTaskDelay(50 / portTICK_PERIOD_MS);
		//read bytes (max 1023) until 20ms pause is happening
		int len = uart_read_bytes(UART_NUM_1, receivedData, sizeof(receivedData), 20 / portTICK_PERIOD_MS);
		uart_flush_input(UART_NUM_1);
		if (len < 1) continue;
		switch (len){

			case sizeof(uartData_test_t):
				dataTest = serialData2Struct<uartData_test_t>(receivedData);
				ESP_LOGW(TAG, "received uartDataStruct len=%d DATA: timestamp=%d, id=%d, value=%.1f", len, dataTest.timestamp, dataTest.id, dataTest.value);
				break;

			case sizeof(motorCommands_t):
				dataMotorCommands = serialData2Struct<motorCommands_t>(receivedData);
				ESP_LOGI(TAG, "received motorCommands struct len=%d left=%.2f%% right=%.2f%%, update target...", len, dataMotorCommands.left.duty, dataMotorCommands.right.duty);
				//update target motor state and duty
				motorLeft.setTarget(dataMotorCommands.left.state, 
						dataMotorCommands.left.duty);
				motorRight.setTarget(dataMotorCommands.right.state, 
						dataMotorCommands.right.duty);
				break;

				//TODO add other received structs here
			default:
				ESP_LOGE(TAG, "received data len=%d cant be associated with configures struct", len);
				break;
		}
	}
}



//=============================
//======= task_uartSend =======
//=============================
//TODO copy send task from board_control/uart.cpp
void task_uartSend(void *arg){
	ESP_LOGW(TAG, "send task started");
	while (1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);    
	}
}
