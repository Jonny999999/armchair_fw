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
//========== ISR onEncoderChange ==========
//=========================================
//handle gpio edge event
//determines direction and rotational speed with a speedSensor object
void IRAM_ATTR onEncoderChange(void* arg) {
	speedSensor* sensor = (speedSensor*)arg;
	int currentState = gpio_get_level(sensor->config.gpioPin);

	//detect rising edge LOW->HIGH (reached end of gap in encoder disk)
	if (currentState == 1 && sensor->prevState == 0) {
		//time since last edge in us
		uint32_t currentTime = esp_timer_get_time();
		uint32_t timeElapsed = currentTime - sensor->lastEdgeTime;
		sensor->lastEdgeTime = currentTime; //update last edge time

		//store duration of last pulse
		sensor->pulseDurations[sensor->pulseCounter] = timeElapsed;
		sensor->pulseCounter++;

		//check if 3rd pulse has occoured
		if (sensor->pulseCounter >= 3) {
			sensor->pulseCounter = 0; //reset counter

			//simplify variable names
			uint32_t pulse1 = sensor->pulseDurations[0];
			uint32_t pulse2 = sensor->pulseDurations[1];
			uint32_t pulse3 = sensor->pulseDurations[2];

			//find shortest pulse
			uint32_t shortestPulse = min(pulse1, min(pulse2, pulse3));

			//Determine direction based on pulse order
			int directionNew = 0;
			if (shortestPulse == pulse1) { //short-medium-long...
				directionNew = 1; //fwd
			} else if (shortestPulse == pulse3) { //long-medium-short...
				directionNew = -1; //rev
			} else if (shortestPulse == pulse2) {
				if (pulse1 < pulse3){ //medium short long-medium-short long...
					directionNew = -1; //rev
				} else { //long short-medium-long short-medium-long...
					directionNew = 1; //fwd
				}
			}

			//save and invert direction if necessay
			//TODO mutex?
			if (sensor->config.directionInverted) sensor->direction = -directionNew;
			else sensor->direction = directionNew;

			//calculate rotational speed
			uint64_t pulseSum = pulse1 + pulse2 + pulse3;
			sensor->currentRpm = directionNew * (sensor->config.degreePerGroup / 360.0 * 60.0 / ((double)pulseSum / 1000000.0));
		}
	}
	//store current pin state for next edge detection
	sensor->prevState = currentState;
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
	gpio_set_intr_type(config.gpioPin, GPIO_INTR_ANYEDGE);
	if (!isrIsInitialized) {
		gpio_install_isr_service(0);
		isrIsInitialized = true;
		ESP_LOGW(TAG, "Initialized ISR service");
	}
	gpio_isr_handler_add(config.gpioPin, onEncoderChange, this);
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
		ESP_LOGW(TAG, "%s - timeout: no pulse within %dms... last pulse was %dms ago => set RPM to 0",
				config.logName, TIMEOUT_NO_ROTATION, timeElapsed/1000);
		currentRpm = 0;
	}
	//debug output (also log variables when this function is called)
	ESP_LOGI(TAG, "%s - getRpm: returning stored rpm=%.3f", config.logName, currentRpm);
	ESP_LOGV(TAG, "%s - rpm=%f, dir=%d, pulseCount=%d, p1=%d, p2=%d, p3=%d lastEdgetime=%d",
			config.logName,
			currentRpm, 
			direction, 
			pulseCounter, 
			(int)pulseDurations[0]/1000,  
			(int)pulseDurations[1]/1000, 
			(int)pulseDurations[2]/1000,
			(int)lastEdgeTime);

	//return currently stored rpm
	return currentRpm;
}



//==========================
//========= getKmph =========
//==========================
//get speed in kilometers per hour
float speedSensor::getKmph(){
	float currentSpeed = getRpm() * config.tireCircumferenceMeter * 60/1000;
	ESP_LOGI(TAG, "%s - getKmph: returning speed=%.3fkm/h", config.logName, currentSpeed);
	return currentSpeed;
}


//==========================
//========= getMps =========
//==========================
//get speed in meters per second
float speedSensor::getMps(){
	float currentSpeed = getRpm() * config.tireCircumferenceMeter;
	ESP_LOGI(TAG, "%s - getMps: returning speed=%.3fm/s", config.logName, currentSpeed);
	return currentSpeed;
}
