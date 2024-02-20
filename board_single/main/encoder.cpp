extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "encoder.h"
}

#include "encoder.hpp"

//-------------------------
//------- variables -------
//-------------------------
static const char * TAG = "encoder";



//==================================
//========== encoder_init ==========
//==================================
//initialize encoder //TODO pass config to this function
QueueHandle_t encoder_init(rotary_encoder_t * encoderConfig)
{
	QueueHandle_t encoderQueue = xQueueCreate(QUEUE_SIZE, sizeof(rotary_encoder_event_t));
	rotary_encoder_init(encoderQueue);
	rotary_encoder_add(encoderConfig);
	if (encoderQueue == NULL)
		ESP_LOGE(TAG, "Error initializing encoder or queue");
	else
		ESP_LOGW(TAG, "Initialized encoder and encoderQueue");
	return encoderQueue;
}



//==================================
//====== task_encoderExample =======
//==================================
//receive and handle all available encoder events
void task_encoderExample(void * arg) {
	//get queue with encoder events from task parameter:
	QueueHandle_t encoderQueue = (QueueHandle_t)arg;
	static rotary_encoder_event_t ev; //store event data
	while (1) {
		if (xQueueReceive(encoderQueue, &ev, portMAX_DELAY)) {
			//log enocder events
			switch (ev.type){
				case RE_ET_CHANGED:
					ESP_LOGI(TAG, "Event type: RE_ET_CHANGED, diff: %d", ev.diff);
					break;
				case RE_ET_BTN_PRESSED:
					ESP_LOGI(TAG, "Button pressed");
					break;
				case RE_ET_BTN_RELEASED:
					ESP_LOGI(TAG, "Button released");
					break;
				case RE_ET_BTN_CLICKED:
					ESP_LOGI(TAG, "Button clicked");
					break;
				case RE_ET_BTN_LONG_PRESSED:
					ESP_LOGI(TAG, "Button long-pressed");
					break;
				default:
					ESP_LOGW(TAG, "Unknown event type");
					break;
			}
		}
	}
}

