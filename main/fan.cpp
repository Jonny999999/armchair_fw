extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

#include "fan.hpp"


//tag for logging
static const char * TAG = "fan-control";


//-----------------------------
//-------- constructor --------
//-----------------------------
controlledFan::controlledFan (fan_config_t config_f, controlledMotor* motor1_f, controlledMotor* motor2_f ){
	//copy config
	config = config_f;
	//copy pointer to motor objects
	motor1 = motor1_f;
	motor2 = motor2_f;

	//initialize gpio pin
	gpio_pad_select_gpio(config.gpio_fan);
	gpio_set_direction(config.gpio_fan, GPIO_MODE_OUTPUT);
}



//--------------------------
//--------- handle ---------
//--------------------------
void controlledFan::handle(){
	//get current state of the motor
	motor1Status = motor1->getStatus();
	motor2Status = motor2->getStatus();

	//update timestamp if any threshold exceeded
	if (motor1Status.duty > config.dutyThreshold
			|| motor2Status.duty > config.dutyThreshold){ //TODO add temperature threshold
		if (!needsCooling){
			timestamp_needsCoolingSet = esp_log_timestamp();
			needsCooling = true;
		}
		timestamp_lastThreshold = esp_log_timestamp();
	} else {
		needsCooling = false;
	}


	//turn off condition
	if (fanRunning
			&& !needsCooling //no more cooling required
			&& (motor1Status.duty == 0) && (motor2Status.duty == 0) //both motors are off 
			   //-> keeps fans running even when lower than threshold already, however turnOffDelay already started TODO: start turn off delay after motor stop only?
			&& (esp_log_timestamp() - timestamp_lastThreshold) > config.turnOffDelayMs){ //turn off delay passed
		fanRunning = false;
		gpio_set_level(config.gpio_fan, 0);        
		timestamp_turnedOff = esp_log_timestamp();
		ESP_LOGI(TAG, "turned fan OFF gpio=%d, minOnMs=%d, WasOnMs=%d", (int)config.gpio_fan, config.minOnMs, esp_log_timestamp()-timestamp_turnedOn);
	}

	//turn on condition
	if (!fanRunning
			&& needsCooling
			&& ((esp_log_timestamp() - timestamp_turnedOff) > config.minOffMs) //fans off long enough
			&& ((esp_log_timestamp() - timestamp_needsCoolingSet) > config.minOnMs)){ //motors on long enough
		fanRunning = true;
		gpio_set_level(config.gpio_fan, 1);        
		timestamp_turnedOn = esp_log_timestamp();
		ESP_LOGI(TAG, "turned fan ON gpio=%d, minOffMs=%d,  WasOffMs=%d", (int)config.gpio_fan, config.minOffMs, esp_log_timestamp()-timestamp_turnedOff);
	}

	//TODO Add statemachine for more specific control? Exponential average?
	//TODO idea: try other aproach? increment a variable with certain weights e.g. integrate over duty, then turn fans on and decrement the variable again
	
	ESP_LOGD(TAG, "fanState=%d, duty1=%f, duty2=%f, needsCooling=%d", fanRunning, motor1Status.duty, motor2Status.duty, needsCooling);
}



