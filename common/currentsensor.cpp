extern "C" {
#include "hal/timer_types.h"
#include "esp_log.h"
}

#include <math.h>
#include "currentsensor.hpp"

//tag for logging
static const char * TAG = "current-sensors";



//--------------------------
//------- getVoltage -------
//--------------------------
//local function to get average voltage from adc
float getVoltage(adc1_channel_t adc, uint32_t samples){
	//measure voltage
	int measure = 0;
	for (int j=0; j<samples; j++){
		measure += adc1_get_raw(adc);
		ets_delay_us(50);
	}
	return (float)measure / samples / 4096 * 3.3;
}



//=============================
//======== constructor ========
//=============================
currentSensor::currentSensor (adc1_channel_t adcChannel_f, float ratedCurrent_f, float snapToZeroThreshold_f, bool isInverted_f){
	//copy config
	adcChannel = adcChannel_f;
	ratedCurrent = ratedCurrent_f;
	isInverted = isInverted_f;
	snapToZeroThreshold = snapToZeroThreshold_f;
	//init adc
	adc1_config_width(ADC_WIDTH_BIT_12); //max resolution 4096
	adc1_config_channel_atten(adcChannel, ADC_ATTEN_DB_11); //max voltage
	//calibrate
	calibrateZeroAmpere();
}



//============================
//=========== read ===========
//============================
float currentSensor::read(void){
	//measure voltage
	voltage = getVoltage(adcChannel, 30);

	//scale voltage to current
	if (voltage < centerVoltage){
		current = (1 - voltage / centerVoltage) * -ratedCurrent;
	} else if (voltage > centerVoltage){
		current = (voltage - centerVoltage) / (3.3 - centerVoltage) * ratedCurrent;
	}else {
		current = 0;
	}

	if (fabs(current) < snapToZeroThreshold)
	{
		ESP_LOGD(TAG, "current=%.3f < threshold=%.3f -> snap to 0", current, snapToZeroThreshold);
		current = 0;
	}
	// invert calculated current if necessary
	else if (isInverted)
		current = -current;

	ESP_LOGI(TAG, "read sensor adc=%d: voltage=%.3fV, centerVoltage=%.3fV => current=%.3fA", (int)adcChannel, voltage, centerVoltage, current);
	return current;
}



//===============================
//===== calibrateZeroAmpere =====
//===============================
void currentSensor::calibrateZeroAmpere(void){
	//measure voltage
	float prev = centerVoltage;
	centerVoltage = getVoltage(adcChannel, 100);
	ESP_LOGW(TAG, "defined centerVoltage (0A) to %.3f (previous %.3f)", centerVoltage, prev);
}
