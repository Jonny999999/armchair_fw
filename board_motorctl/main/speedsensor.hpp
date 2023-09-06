#pragma once
extern "C" {
#include "esp_log.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "esp_timer.h"
}

//Encoder disk requirements:
//encoder disk has to have gaps in 3 differnt intervals (short, medium, long)
//that pattern can be repeated multiple times, see config option
typedef struct {
    gpio_num_t gpioPin;
	float degreePerGroup;	//360 / [count of short,medium,long groups on encoder disk]
	float tireCircumferenceMeter;
	//positive direction is pulse order "short, medium, long"
	bool directionInverted;
	char* logName;
} speedSensor_config_t;


class speedSensor {
	//TODO add count of revolutions/pulses if needed? (get(), reset() etc)
public:
	//constructor
    speedSensor(speedSensor_config_t config);
 //initializes gpio pin and configures interrupt
    void init();
	
	//negative values = reverse direction
	//positive values = forward direction
	float getKmph(); //kilometers per hour
	float getMps(); //meters per second
	float getRpm();  //rotations per minute

	//1=forward, -1=reverse
    int direction;

	//variables for handling the encoder
	speedSensor_config_t config;
    int prevState = 0;
	uint64_t pulseDurations[3] = {};
	uint64_t lastEdgeTime = 0;
	uint8_t pulseCounter = 0;
	int debugCount = 0;
	double currentRpm = 0;

private:

};



