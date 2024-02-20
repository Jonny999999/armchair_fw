#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
}

#include "motordrivers.hpp"
#include "currentsensor.hpp"


//=======================================
//====== struct/type  declarations ======
//=======================================
//outsourced to common/types.hpp
#include "types.hpp"

typedef void (*motorSetCommandFunc_t)(motorCommand_t cmd);


//===================================
//====== controlledMotor class ======
//===================================
class controlledMotor {
    public:
        //--- functions ---
        controlledMotor(motorSetCommandFunc_t setCommandFunc,  motorctl_config_t config_control, nvs_handle_t * nvsHandle); //constructor with structs for configuring motordriver and parameters for control TODO: add configuration for currentsensor
        void handle(); //controls motor duty with fade and current limiting feature (has to be run frequently by another task)
        void setTarget(motorstate_t state_f, float duty_f = 0); //adds target command to queue for handle function
        motorCommand_t getStatus(); //get current status of the motor (returns struct with state and duty)

        uint32_t getFade(fadeType_t fadeType); //get currently set acceleration or deceleration fading time
        uint32_t getFadeDefault(fadeType_t fadeType); //get acceleration or deceleration fading time from config
        void setFade(fadeType_t fadeType, bool enabled); //enable/disable acceleration or deceleration fading
        void setFade(fadeType_t fadeType, uint32_t msFadeNew); //set acceleration or deceleration fade time
        bool toggleFade(fadeType_t fadeType); //toggle acceleration or deceleration on/off

        float getCurrentA() {return cSensor.read();}; //read current-sensor of this motor (Ampere)
											  
		//TODO set current limit method

    private:
        //--- functions ---
        void init(); // creates command-queue and initializes config values
        void loadAccelDuration(void); // load stored value for msFadeAccel from nvs
        void loadDecelDuration(void);
        void writeAccelDuration(uint32_t newValue); // write value to nvs and update local variable
        void writeDecelDuration(uint32_t newValue);

        //--- objects ---
        //queue for sending commands to the separate task running the handle() function very fast
        QueueHandle_t commandQueue = NULL;
		//current sensor
		currentSensor cSensor;

		//function pointer that sets motor duty (driver)
		motorSetCommandFunc_t motorSetCommand;

        //--- variables ---
        //TODO add name for logging?
        //struct for storing control specific parameters
        motorctl_config_t config;
        motorstate_t state = motorstate_t::IDLE;
        //handle for using the nvs flash (persistent config variables)
        nvs_handle_t * nvsHandle;

        float currentMax;
        float currentNow;

        float dutyTarget = 0;
        float dutyNow = 0;
        float dutyIncrementAccel;
        float dutyIncrementDecel;
        float dutyDelta;

        uint32_t msFadeAccel;
        uint32_t msFadeDecel;

        uint32_t ramp;
        int64_t timestampLastRunUs = 0;

		bool deadTimeWaiting = false;
		uint32_t timestampsModeLastActive[4] = {};
        motorstate_t statePrev = motorstate_t::FWD;

        struct motorCommand_t commandReceive = {};
        struct motorCommand_t commandSend = {};

		uint32_t timestamp_commandReceived = 0;
		bool receiveTimeout = false;
};



// struct with variables passed to task from main
typedef struct task_motorctl_parameters_t {
    controlledMotor * motorLeft;
    controlledMotor * motorRight;
} task_motorctl_parameters_t;


//====================================
//========== motorctl task ===========
//====================================
//task that inititialized the display, displays welcome message 
//and releatedly updates the display with certain content
//note: pointer to required objects have to be provided as task-parameter
void task_motorctl( void * task_motorctl_parameters );

