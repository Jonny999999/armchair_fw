#include "speedsensor.hpp"
#include "esp_timer.h"
#include <ctime>

//===== config =====
#define TIMEOUT_NO_ROTATION 1000 //RPM set to 0 when no pulses within that time (ms)
static const char* TAG = "speedSensor";


//initialize ISR only once (for multiple instances)
bool speedSensor::isrIsInitialized = false;


uint32_t min(uint32_t a, uint32_t b){
	if (a>b) return b;
	else return a;
}



//=========================================
//========== ISR onEncoderRising ==========
//=========================================
//handle gpio rising edge event
//determines direction and rotational speed with a speedSensor object
void IRAM_ATTR onEncoderRising(void *arg)
{
	speedSensor *sensor = (speedSensor *)arg;
	int currentState = gpio_get_level(sensor->config.gpioPin);

	// time since last edge in us
	uint32_t currentTime = esp_timer_get_time();
	uint32_t timeElapsed = currentTime - sensor->lastEdgeTime;
	sensor->lastEdgeTime = currentTime; // update last edge time

	// store duration of last pulse
	sensor->pulseDurations[sensor->pulseCounter] = timeElapsed;
	sensor->pulseCounter++;

	// check if 3rd pulse has occoured (one sequence recorded)
	if (sensor->pulseCounter >= 3)
	{
		sensor->pulseCounter = 0; // reset count

		// simplify variable names
		uint32_t pulse1 = sensor->pulseDurations[0];
		uint32_t pulse2 = sensor->pulseDurations[1];
		uint32_t pulse3 = sensor->pulseDurations[2];

		// save all recored pulses of this sequence (for logging only)
		sensor->pulse1 = pulse1;
		sensor->pulse2 = pulse2;
		sensor->pulse3 = pulse3;

		// find shortest pulse
		sensor->shortestPulse = min(pulse1, min(pulse2, pulse3));

		// ignore this pulse sequence if one pulse is too short (possible noise)
		if (sensor->shortestPulse < sensor->config.minPulseDurationUs)
		{
			sensor->debug_countIgnoredSequencesTooShort++;
			return;
		}

		//--- Determine direction based on pulse order ---
		int direction = 0;
		if (sensor->shortestPulse == pulse1) // short...
		{
			if (pulse2 < pulse3) // short-medium-long -->
				direction = 1;
			else // short-long-medium <--
				direction = -1;
		}
		else if (sensor->shortestPulse == pulse3) //...short
		{
			if (pulse1 > pulse2) // long-medium-short <--
				direction = -1;
			else // medium-long-short -->
				direction = 1;
		}
		else if (sensor->shortestPulse == pulse2) //...short...
		{
			if (pulse1 < pulse3) // medium-short-long
				direction = -1;
			else // long-short-medium
				direction = 1;
		}

		// save and invert direction if necessay
		if (sensor->config.directionInverted)
			direction = -direction;

		// calculate rotational speed
		uint64_t pulseSum = pulse1 + pulse2 + pulse3;
		sensor->currentRpm = direction * (sensor->config.degreePerGroup / 360.0 * 60.0 / ((double)pulseSum / 1000000.0));
		sensor->timeLastUpdate = currentTime;
	}
}




//============================
//======= constructor ========
//============================
speedSensor::speedSensor(speedSensor_config_t config_f){
	//copy config
	config = config_f;
	//init gpio and ISR
	init();
}



//==========================
//========== init ==========
//==========================
 //initializes gpio pin and configures interrupt
void speedSensor::init() {
	//configure pin
	gpio_pad_select_gpio(config.gpioPin);
	gpio_set_direction(config.gpioPin, GPIO_MODE_INPUT);
	gpio_set_pull_mode(config.gpioPin, GPIO_PULLUP_ONLY);

	//configure interrupt
	gpio_set_intr_type(config.gpioPin, GPIO_INTR_POSEDGE);
	if (!isrIsInitialized) {
		gpio_install_isr_service(0);
		isrIsInitialized = true;
		ESP_LOGW(TAG, "Initialized ISR service");
	}
	gpio_isr_handler_add(config.gpioPin, onEncoderRising, this);
	ESP_LOGW(TAG, "[%s], configured gpio-pin %d and interrupt routine", config.logName, (int)config.gpioPin);
}




//==========================
//========= getRpm =========
//==========================
//get rotational speed in revolutions per minute
float speedSensor::getRpm(){
	uint32_t timeElapsed = esp_timer_get_time() - lastEdgeTime;
	//timeout (standstill)
	//TODO variable timeout considering config.degreePerGroup
	if ((currentRpm != 0) && (esp_timer_get_time() - lastEdgeTime) > TIMEOUT_NO_ROTATION*1000){
		ESP_LOGI(TAG, "%s - timeout: no pulse within %dms... last pulse was %dms ago => set RPM to 0",
				config.logName, TIMEOUT_NO_ROTATION, timeElapsed/1000);
		currentRpm = 0;
	}
	//debug output (also log variables when this function is called)
	ESP_LOGD(TAG, "[%s] getRpm: returning stored rpm=%.3f", config.logName, currentRpm);
	ESP_LOGV(TAG, "[%s] rpm=%f, pulseCount=%d, p1=%d, p2=%d, p3=%d, shortest=%d, totalTooShortCount=%d",
			 config.logName,
			 currentRpm,
			 pulseCounter,
			 pulse1 / 1000,
			 pulse2 / 1000,
			 pulse3 / 1000,
			 shortestPulse / 1000,
			 debug_countIgnoredSequencesTooShort);
	//return currently stored rpm
	return currentRpm;
}



//===========================
//========= getKmph =========
//===========================
//get speed in kilometers per hour
float speedSensor::getKmph(){
	float currentSpeed = getRpm() * config.tireCircumferenceMeter * 60/1000;
	ESP_LOGD(TAG, "%s - getKmph: returning speed=%.3fkm/h", config.logName, currentSpeed);
	return currentSpeed;
}


//==========================
//========= getMps =========
//==========================
//get speed in meters per second
float speedSensor::getMps(){
	float currentSpeed = getRpm() * config.tireCircumferenceMeter / 60;
	ESP_LOGD(TAG, "%s - getMps: returning speed=%.3fm/s", config.logName, currentSpeed);
	return currentSpeed;
}
