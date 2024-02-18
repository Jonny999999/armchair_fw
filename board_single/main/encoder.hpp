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
QueueHandle_t encoder_init(); //TODO pass config to function


//task that handles encoder events
//note: queue obtained from encoder_init() has to be passed to that task
void task_encoderExample(void *encoderQueue);
//example: xTaskCreate(&task_encoderExample, "task_buzzer", 2048, encoderQueue, 2, NULL);