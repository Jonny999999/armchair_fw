#pragma once

#include "uart_common.hpp"

void uart_task_testing(void *arg);
void task_uartReceive(void *arg);
void task_uartSend(void *arg);



typedef struct {
	uint32_t timestamp;
	int id;
	float value;
} uartDataStruct;
