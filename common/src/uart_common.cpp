#include "uart_common.hpp"

static const char * TAG = "uart-common";
SemaphoreHandle_t uart_semaphoreSend = NULL;
bool uart_isInitialized = false;



//===========================
//======== uart_init  =======
//===========================
//initial struct on given pins
//TODO add pins and baud rate to config?
void uart_init(void){
	ESP_LOGW(TAG, "initializing uart1...");
	uart_semaphoreSend = xSemaphoreCreateBinary();
	uart_config_t uart1_config = {
		.baud_rate = 115198,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_EVEN,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	ESP_LOGI(TAG, "configure...");
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart1_config));
	ESP_LOGI(TAG, "setpins...");
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 23, 22, 0, 0));
	ESP_LOGI(TAG, "init...");
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 10, NULL, 0));
	uart_isInitialized = true;
	xSemaphoreGive(uart_semaphoreSend);
}



//===========================
//===== uart_sendBinary  ====
//===========================
//encode byte array to message (start, stop, escape pattern)
//and send it via uart
//note: use sendStruct template in uart_common.hpp
void uart_sendBinary(uint8_t *data, int length) {
	const uint8_t startPattern = START_PATTERN;
	const uint8_t endPattern = END_PATTERN;
	const uint8_t escapeByte = ESCAPE_BYTE;
	ESP_LOGI(TAG, "encoding and sending bytes len=%d", length);
	//wait for last message to finish sending
	if (xSemaphoreTake(uart_semaphoreSend, UART_WRITE_WAIT_TIMEOUT / portTICK_PERIOD_MS) == pdTRUE){
		//send start byte
		uart_write_bytes(UART_NUM_1, &startPattern, 1);

		for (int i = 0; i < length; i++) {
			if (data[i] == START_PATTERN || data[i] == END_PATTERN || data[i] == ESCAPE_BYTE) {
				//add escape byte if next byte is special pattern by accident
				uart_write_bytes(UART_NUM_1, &escapeByte, 1);
			}
			uart_write_bytes(UART_NUM_1, (const char *)&data[i], 1);
		}
		//send end byte
		uart_write_bytes(UART_NUM_1, &endPattern, 1);
		vTaskDelay(UART_WRITE_MIN_DELAY / portTICK_PERIOD_MS);
		xSemaphoreGive(uart_semaphoreSend);
	} else ESP_LOGE(TAG, "timeout waiting for uart write semaphore! dropping data");
}




//================================
//=== uart_processReceivedByte ===
//================================
//decode received message to byte array (start, stop, escape pattern)
//has to be run for each receibed byte
//runs provided messageHandler function when complete message received
uint8_t receivedData[MAX_MESSAGE_LENGTH];
int dataIndex = 0;
bool insideMessage = false;
bool escaped = false;

void uart_processReceivedByte(uint8_t data, messageHandleFunc_t messageHandler){
	ESP_LOGD(TAG, "process byte %x", data);
	if (escaped) {
		//this byte is actual data, no end/start byte
		escaped = false;
		receivedData[dataIndex++] = data;
		if (dataIndex >= MAX_MESSAGE_LENGTH) {
			insideMessage = false;
			dataIndex = 0;
		}
	} else if (data == START_PATTERN) {
		//start of message
		insideMessage = true;
		dataIndex = 0;
	} else if (insideMessage) {
		if (data == ESCAPE_BYTE) {
			ESP_LOGI(TAG, "escape byte received");
			//escape next byte
			escaped = true;
		} else if (data == END_PATTERN) {
			//end of message
			insideMessage = false;
			//call provided function that handles actual messages
			messageHandler(receivedData, dataIndex);
			dataIndex = 0;
		} else if (dataIndex < MAX_MESSAGE_LENGTH - 2) {
			//normal byte - append byte to data
			receivedData[dataIndex++] = data;
		} else {
			//message too long / buffer exceeded
			insideMessage = false;
			dataIndex = 0;
			ESP_LOGE(TAG, "received message too long! dropped");
		}
	} else ESP_LOGE(TAG, "start pattern missing! ignoreing byte");
}
	

