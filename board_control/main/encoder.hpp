extern "C" {
#include "freertos/FreeRTOS.h"  // FreeRTOS related headers
#include "freertos/task.h"
#include "encoder.h"
}

//config
#define QUEUE_SIZE 10
#define PIN_A GPIO_NUM_25
#define PIN_B GPIO_NUM_26
#define PIN_BUTTON GPIO_NUM_27


//init encoder with config in encoder.cpp
void encoder_init();

//task that handles encoder events
void task_encoder(void *arg);
