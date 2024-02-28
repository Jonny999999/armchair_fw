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
	uint32_t minPulseDurationUs; //smallest possible pulse duration (time from start small-pulse to start long-pulse at full speed). Set to 0 to disable this noise detection
	float tireCircumferenceMeter;
	//default positive direction is pulse order "short, medium, long"
	bool directionInverted;
	char* logName;
} speedSensor_config_t;


class speedSensor {
	//TODO add count of revolutions/pulses if needed? (get(), reset() etc)
public:
	//constructor
    speedSensor(speedSensor_config_t config);
	// initializes gpio pin, configures and starts interrupt
	void init();
	
	//negative values = reverse direction
	//positive values = forward direction
	float getKmph(); //kilometers per hour
	float getMps(); //meters per second
	float getRpm();  //rotations per minute
	uint32_t getTimeLastUpdate() {return timeLastUpdate;};

	//variables for handling the encoder (public because ISR needs access)
	speedSensor_config_t config;
	uint32_t pulseDurations[3] = {};
	uint32_t pulse1, pulse2, pulse3;
	uint32_t shortestPulse = 0;
	uint32_t shortestPulsePrev = 0;
	uint32_t lastEdgeTime = 0;
	uint8_t pulseCounter = 0;
	int debugCount = 0;
	uint32_t debug_countIgnoredSequencesTooShort = 0;
	double currentRpm = 0;
	uint32_t timeLastUpdate = 0;

private:
	static bool isrIsInitialized; // default false due to static
};



