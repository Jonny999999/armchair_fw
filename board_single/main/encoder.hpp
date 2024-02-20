extern "C" {
#include "freertos/FreeRTOS.h"  // FreeRTOS related headers
#include "freertos/task.h"
#include "encoder.h"
}

//config
#define QUEUE_SIZE 10

//init encoder with pointer to encoder config
QueueHandle_t encoder_init(rotary_encoder_t * encoderConfig);


//task that handles encoder events
//note: queue obtained from encoder_init() has to be passed to that task
void task_encoderExample(void *encoderQueue);
//example: xTaskCreate(&task_encoderExample, "task_buzzer", 2048, encoderQueue, 2, NULL);