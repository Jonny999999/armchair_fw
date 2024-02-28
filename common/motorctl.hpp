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
#include "speedsensor.hpp"


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
        //TODO move speedsensor object creation in this class to (pass through / wrap methods)
        controlledMotor(motorSetCommandFunc_t setCommandFunc,  motorctl_config_t config_control, nvs_handle_t * nvsHandle, speedSensor * speedSensor, controlledMotor ** otherMotor); //constructor with structs for configuring motordriver and parameters for control TODO: add configuration for currentsensor
        void handle(); //controls motor duty with fade and current limiting feature (has to be run frequently by another task)
        void setTarget(motorstate_t state_f, float duty_f = 0); //adds target command to queue for handle function
        void setTarget(motorCommand_t command); 
        motorCommand_t getStatus(); //get current status of the motor (returns struct with state and duty)
        float getDuty() {return dutyNow;};
        float getTargetDuty() {return dutyTarget;};
        float getTargetSpeed() {return speedTarget;};
        void enableTractionControlSystem() {config.tractionControlSystemEnabled = true;};
        void disableTractionControlSystem() {config.tractionControlSystemEnabled = false; tcs_isExceeded = false;};
        bool getTractionControlSystemStatus() {return config.tractionControlSystemEnabled;};

        uint32_t getFade(fadeType_t fadeType); //get currently set acceleration or deceleration fading time
        uint32_t getFadeDefault(fadeType_t fadeType); //get acceleration or deceleration fading time from config
        void setFade(fadeType_t fadeType, bool enabled); //enable/disable acceleration or deceleration fading
        void setFade(fadeType_t fadeType, uint32_t msFadeNew); //set acceleration or deceleration fade time
        bool toggleFade(fadeType_t fadeType); //toggle acceleration or deceleration on/off

        float getCurrentA() {return cSensor.read();}; //read current-sensor of this motor (Ampere)
        char * getName() const {return config.name;};
											  
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
        //speed sensor
        speedSensor * sSensor;
        //other motor (needed for traction control)
        controlledMotor ** ppOtherMotor; //ptr to ptr of controlledMotor (because not created at initialization yet)


		//function pointer that sets motor duty (driver)
		motorSetCommandFunc_t motorSetCommand;

        //--- variables ---
        //TODO add name for logging?
        //struct for storing control specific parameters
        motorctl_config_t config;
        motorstate_t state = motorstate_t::IDLE;
        motorControlMode_t mode = motorControlMode_t::DUTY;
        //handle for using the nvs flash (persistent config variables)
        nvs_handle_t * nvsHandle;

        float currentMax;
        float currentNow;

        //speed mode
        float speedTarget = 0;
        float speedNow = 0;


        float dutyTarget = 0;
        float dutyNow = 0;

        float dutyIncrementAccel;
        float dutyIncrementDecel;
        float dutyDelta;
        uint32_t timeoutWaitForCommand = 0;

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

        //traction control system
        uint32_t tcs_timestampLastSpeedUpdate = 0; //track speedsensor update
        int64_t tcs_timestampBeginExceeded = 0; //track start of event
        uint32_t tcs_usExceeded = 0; //sum up time
        bool tcs_isExceeded = false; //is currently too fast
        int64_t tcs_timestampLastRun = 0;
};

//====================================
//========== motorctl task ===========
//====================================
// note: pointer to a 'controlledMotor' object has to be provided as task-parameter
// runs handle method of certain motor repeatedly: 
// receives commands from control via queue, handle ramp and current, apply new duty by passing it to method of motordriver (ptr)
void task_motorctl( void * controlledMotor );
