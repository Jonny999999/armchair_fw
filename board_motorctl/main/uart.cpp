#include "uart.hpp"
#include "config.hpp"
#include "types.hpp"
#include "uart_common.hpp"
//===== uart board MOTORCTL =====

static const char * TAG = "uart";



//handle received payload from uart
void handleMessage(uint8_t *receivedData, int len) {
	ESP_LOGI(TAG, "complete message received len=%d", len);
	//local variables
	uartData_test_t dataTest;
	motorCommands_t dataMotorCommands;
	//assign data to struct
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



//==============================
//====== task_uartReceive ======
//==============================
//TODO duplicate code, same task in both boards, only handleMessage function has to be passed -> move to uart_common
void task_uartReceive(void *arg){
	ESP_LOGW(TAG, "receive task started");
	while (1) {
		uint8_t byte;
		//read 1 byte TODO: use uart queue here? data might get lost when below function takes longer than data arrives
		int len = uart_read_bytes(UART_NUM_1, &byte, 1, portMAX_DELAY);
		if (len > 0) {
			//process received byte
			uart_processReceivedByte(byte, handleMessage);
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
