#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <driver/adc.h>
}
//=======================================
//====== struct/type  declarations ======
//=======================================
//global structs and types that need to be available for all boards
//this file is necessary to prevent dependency loop between motordrivers.hpp and motorctl.hpp since 


//===============================
//==== from motordrivers.hpp ====
//===============================
enum class motorstate_t {IDLE, FWD, REV, BRAKE};
//definition of string array to be able to convert state enum to readable string (defined in motordrivers.cpp)
extern const char* motorstateStr[4];

enum class motorControlMode_t {DUTY, CURRENT, SPEED};


//===========================
//==== from motorctl.hpp ====
//===========================
//struct for sending command for one motor in the queue
struct motorCommand_t {
    motorstate_t state;
    float duty;
};

//struct containing commands for two motors
typedef struct motorCommands_t {
    motorCommand_t left;
    motorCommand_t right;
} motorCommands_t;

//struct with all config parameters for a motor regarding ramp and current limit
typedef struct motorctl_config_t {
    char * name;    //name for unique nvs storage-key prefix and logging
    uint32_t msFadeAccel; //acceleration of the motor (ms it takes from 0% to 100%)
    uint32_t msFadeDecel; //deceleration of the motor (ms it takes from 100% to 0%)
	bool currentLimitEnabled;
    bool tractionControlSystemEnabled;
	adc1_channel_t currentSensor_adc;
	float currentSensor_ratedCurrent;
    float currentMax;
    bool currentInverted;
    float currentSnapToZeroThreshold;
	uint32_t deadTimeMs; //time motor stays in IDLE before direction change
} motorctl_config_t;

//enum fade type (acceleration, deceleration)
//e.g. used for specifying which fading should be modified with setFade, togleFade functions
enum class fadeType_t {ACCEL, DECEL};

