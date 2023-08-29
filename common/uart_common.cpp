#include "uart_common.hpp"

static const char * TAG = "uart-common";


//===========================
//======== uart_init  =======
//===========================
//initial struct on given pins
//TODO add pins and baud rate to config?
void uart_init(void){
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
}


	

