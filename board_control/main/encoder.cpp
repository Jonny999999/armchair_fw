	#include "encoder.h"
extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
}

#include "encoder.hpp"

//-------------------------
//------- variables -------
//-------------------------
static const char * TAG = "encoder";
uint16_t encoderCount;
rotary_encoder_btn_state_t encoderButtonState = {};
QueueHandle_t encoderQueue = NULL;

//encoder config
rotary_encoder_t encoderConfig = {
	.pin_a = PIN_A,
	.pin_b = PIN_B,
	.pin_btn = PIN_BUTTON,
	.code = 1,
	.store = encoderCount,
	.index = 0,
	.btn_pressed_time_us = 20000,
	.btn_state = encoderButtonState
};



//==================================
//========== encoder_init ==========
//==================================
//initialize encoder
void encoder_init(){
	encoderQueue = xQueueCreate(QUEUE_SIZE, sizeof(rotary_encoder_event_t));
	rotary_encoder_init(encoderQueue);
	rotary_encoder_add(&encoderConfig);
}



//==================================
//========== task_encoder ==========
//==================================
//receive and handle encoder events
void task_encoder(void *arg) {
	rotary_encoder_event_t ev; //store event data
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

