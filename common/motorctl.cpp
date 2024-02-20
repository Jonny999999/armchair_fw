#include "motorctl.hpp"
#include "esp_log.h"
#include "types.hpp"

//tag for logging
static const char * TAG = "motor-control";

#define TIMEOUT_IDLE_WHEN_NO_COMMAND 8000



//====================================
//========== motorctl task ===========
//====================================
//task for handling the motors (ramp, current limit, driver)
void task_motorctl( void * task_motorctl_parameters ){
	task_motorctl_parameters_t *objects = (task_motorctl_parameters_t *)task_motorctl_parameters;
    ESP_LOGW(TAG, "Task-motorctl: starting handle loop...");
    while(1){
    	objects->motorRight->handle();
    	objects->motorLeft->handle();
        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}



//=============================
//======== constructor ========
//=============================
//constructor, simultaniously initialize instance of motor driver 'motor' and current sensor 'cSensor' with provided config (see below lines after ':')
controlledMotor::controlledMotor(motorSetCommandFunc_t setCommandFunc,  motorctl_config_t config_control): 
	cSensor(config_control.currentSensor_adc, config_control.currentSensor_ratedCurrent) {
		//copy parameters for controlling the motor
		config = config_control;
		//pointer to update motot dury method
		motorSetCommand = setCommandFunc;
		//copy configured default fading durations to actually used variables
		msFadeAccel = config.msFadeAccel;
		msFadeDecel = config.msFadeDecel;

		init();
		//TODO: add currentsensor object here
		//currentSensor cSensor(config.currentSensor_adc, config.currentSensor_ratedCurrent);
	}



//============================
//========== init ============
//============================
void controlledMotor::init(){
    commandQueue = xQueueCreate( 1, sizeof( struct motorCommand_t ) );
    if (commandQueue == NULL)
    ESP_LOGE(TAG, "Failed to create command-queue");
    else
    ESP_LOGW(TAG, "Initialized command-queue");

	//cSensor.calibrateZeroAmpere(); //currently done in currentsensor constructor TODO do this regularly e.g. in idle?
}



//----------------
//----- fade -----
//----------------
//local function that fades a variable
//- increments a variable (pointer) by given value
//- sets to target if already closer than increment
//TODO this needs testing
void fade(float * dutyNow, float dutyTarget, float dutyIncrement){
    float dutyDelta = dutyTarget - *dutyNow; 
    if ( fabs(dutyDelta) > fabs(dutyIncrement) ) { //check if already close to target
        *dutyNow = *dutyNow + dutyIncrement;
    }
    //already closer to target than increment
    else {
        *dutyNow = dutyTarget;
    }
}



//----------------------------
//----- getStateFromDuty -----
//----------------------------
//local function that determines motor the direction from duty range -100 to 100
motorstate_t getStateFromDuty(float duty){
	if(duty > 0) return motorstate_t::FWD;
	if (duty < 0) return motorstate_t::REV;
	return motorstate_t::IDLE;
}



//==============================
//=========== handle ===========
//==============================
//function that controls the motor driver and handles fading/ramp, current limit and deadtime
void controlledMotor::handle(){

    //TODO: History: skip fading when motor was running fast recently / alternatively add rot-speed sensor

    //--- receive commands from queue ---
    if( xQueueReceive( commandQueue, &commandReceive, ( TickType_t ) 0 ) )
    {
        ESP_LOGD(TAG, "Read command from queue: state=%s, duty=%.2f", motorstateStr[(int)commandReceive.state], commandReceive.duty);
        state = commandReceive.state;
        dutyTarget = commandReceive.duty;
		receiveTimeout = false;
		timestamp_commandReceived = esp_log_timestamp();

        //--- convert duty ---
        //define target duty (-100 to 100) from provided duty and motorstate
        //this value is more suitable for the fading algorithm
        switch(commandReceive.state){
            case motorstate_t::BRAKE:
                //update state
                state = motorstate_t::BRAKE;
                //dutyTarget = 0;
                dutyTarget = fabs(commandReceive.duty);
                break;
            case motorstate_t::IDLE:
                dutyTarget = 0;
                break;
            case motorstate_t::FWD:
                dutyTarget = fabs(commandReceive.duty);
                break;
            case motorstate_t::REV:
                dutyTarget = - fabs(commandReceive.duty);
                break;
        }
    }

	//--- timeout, no data ---
	//turn motors off if no data received for long time (e.g. no uart data / control offline)
	//TODO no timeout when braking?
	if ((esp_log_timestamp() - timestamp_commandReceived) > TIMEOUT_IDLE_WHEN_NO_COMMAND && !receiveTimeout){
		receiveTimeout = true;
		state = motorstate_t::IDLE;
		dutyTarget = 0;
		ESP_LOGE(TAG, "TIMEOUT, no target data received for more than %ds -> switch to IDLE", TIMEOUT_IDLE_WHEN_NO_COMMAND/1000);
	}

    //--- calculate increment ---
    //calculate increment for fading UP with passed time since last run and configured fade time
    int64_t usPassed = esp_timer_get_time() - timestampLastRunUs;
    if (msFadeAccel > 0){
    dutyIncrementAccel = ( usPassed / ((float)msFadeAccel * 1000) ) * 100; //TODO define maximum increment - first run after startup (or long) pause can cause a very large increment
    } else {
        dutyIncrementAccel = 100;
    }

    //calculate increment for fading DOWN with passed time since last run and configured fade time
    if (msFadeDecel > 0){
        dutyIncrementDecel = ( usPassed / ((float)msFadeDecel * 1000) ) * 100; 
    } else {
        dutyIncrementDecel = 100;
    }


	//--- BRAKE ---
	//brake immediately, update state, duty and exit this cycle of handle function
	if (state == motorstate_t::BRAKE){
		ESP_LOGD(TAG, "braking - skip fading");
		motorSetCommand({motorstate_t::BRAKE, dutyTarget});
		ESP_LOGI(TAG, "Set Motordriver: state=%s, duty=%.2f - Measurements: current=%.2f, speed=N/A", motorstateStr[(int)state], dutyNow, currentNow);
		//dutyNow = 0;
		return; //no need to run the fade algorithm
	}


    //--- calculate difference ---
    dutyDelta = dutyTarget - dutyNow;
    //positive: need to increase by that value
    //negative: need to decrease


	//----- FADING -----
    //fade duty to target (up and down)
    //TODO: this needs optimization (can be more clear and/or simpler)
    if (dutyDelta > 0) { //difference positive -> increasing duty (-100 -> 100)
        if (dutyNow < 0) { //reverse, decelerating
            fade(&dutyNow, dutyTarget, dutyIncrementDecel);
        }
        else if (dutyNow >= 0) { //forward, accelerating
            fade(&dutyNow, dutyTarget, dutyIncrementAccel);
        }
    }
    else if (dutyDelta < 0) { //difference negative -> decreasing duty (100 -> -100)
        if (dutyNow <= 0) { //reverse, accelerating
            fade(&dutyNow, dutyTarget, - dutyIncrementAccel);
        }
        else if (dutyNow > 0) { //forward, decelerating
            fade(&dutyNow, dutyTarget, - dutyIncrementDecel);
        }
    }


	//----- CURRENT LIMIT -----
	currentNow = cSensor.read();
	if ((config.currentLimitEnabled) && (dutyDelta != 0)){
		if (fabs(currentNow) > config.currentMax){
			float dutyOld = dutyNow;
			//adaptive decrement:
			//Note current exceeded twice -> twice as much decrement: TODO: decrement calc needs finetuning, currently random values
			dutyIncrementDecel = (currentNow/config.currentMax) * ( usPassed / ((float)msFadeDecel * 1500) ) * 100; 
			float currentLimitDecrement = ( (float)usPassed / ((float)1000 * 1000) ) * 100; //1000ms from 100 to 0
			if (dutyNow < -currentLimitDecrement) {
				dutyNow += currentLimitDecrement;
			} else if (dutyNow > currentLimitDecrement) {
				dutyNow -= currentLimitDecrement;
			}
			ESP_LOGW(TAG, "current limit exceeded! now=%.3fA max=%.1fA => decreased duty from %.3f to %.3f", currentNow, config.currentMax, dutyOld, dutyNow);
		}
	}

	
    //--- define new motorstate --- (-100 to 100 => direction)
	state=getStateFromDuty(dutyNow);


	//--- DEAD TIME ----
	//ensure minimum idle time between direction change to prevent driver overload
	//FWD -> IDLE -> FWD  continue without waiting
	//FWD -> IDLE -> REV  wait for dead-time in IDLE
	//TODO check when changed only?
	if (	//not enough time between last direction state
			(   state == motorstate_t::FWD && (esp_log_timestamp() - timestampsModeLastActive[(int)motorstate_t::REV] < config.deadTimeMs))
			|| (state == motorstate_t::REV && (esp_log_timestamp() - timestampsModeLastActive[(int)motorstate_t::FWD] < config.deadTimeMs))
	   ){
		ESP_LOGD(TAG, "waiting dead-time... dir change %s -> %s", motorstateStr[(int)statePrev], motorstateStr[(int)state]);
		if (!deadTimeWaiting){ //log start
			deadTimeWaiting = true;
			ESP_LOGW(TAG, "starting dead-time... %s -> %s", motorstateStr[(int)statePrev], motorstateStr[(int)state]);
		}
		//force IDLE state during wait
		state = motorstate_t::IDLE;
		dutyNow = 0;
	} else {
		if (deadTimeWaiting){ //log end
			deadTimeWaiting = false;
			ESP_LOGW(TAG, "dead-time ended - continue with %s", motorstateStr[(int)state]);
		}
		ESP_LOGV(TAG, "deadtime: no change below deadtime detected... dir=%s, duty=%.1f", motorstateStr[(int)state], dutyNow);
	}
			

	//--- save current actual motorstate and timestamp ---
	//needed for deadtime
	timestampsModeLastActive[(int)getStateFromDuty(dutyNow)] = esp_log_timestamp();
	//(-100 to 100 => direction)
	statePrev = getStateFromDuty(dutyNow);


    //--- apply new target to motor ---
    motorSetCommand({state, (float)fabs(dutyNow)});
	ESP_LOGI(TAG, "Set Motordriver: state=%s, duty=%.2f - Measurements: current=%.2f, speed=N/A", motorstateStr[(int)state], dutyNow, currentNow);
    //note: BRAKE state is handled earlier
    

    //--- update timestamp ---
    timestampLastRunUs = esp_timer_get_time(); //update timestamp last run with current timestamp in microseconds
}



//===============================
//========== setTarget ==========
//===============================
//function to set the target mode and duty of a motor
//puts the provided command in a queue for the handle function running in another task
void controlledMotor::setTarget(motorstate_t state_f, float duty_f){
    commandSend = {
        .state = state_f,
        .duty = duty_f
    };
    ESP_LOGI(TAG, "setTarget: Inserting command to queue: state='%s'(%d), duty=%.2f", motorstateStr[(int)commandSend.state], (int)commandSend.state, commandSend.duty);
    //send command to queue (overwrite if an old command is still in the queue and not processed)
    xQueueOverwrite( commandQueue, ( void * )&commandSend);
    //xQueueSend( commandQueue, ( void * )&commandSend, ( TickType_t ) 0 );
    ESP_LOGD(TAG, "finished inserting new command");

}



//===============================
//========== getStatus ==========
//===============================
//function which returns the current status of the motor in a motorCommand_t struct
motorCommand_t controlledMotor::getStatus(){
    motorCommand_t status = {
        .state = state,
        .duty = dutyNow
    };
    //TODO: mutex
    return status;
};



//===============================
//=========== getFade ===========
//===============================
//return currently configured accel / decel time
uint32_t controlledMotor::getFade(fadeType_t fadeType){
    switch(fadeType){
        case fadeType_t::ACCEL:
            return msFadeAccel;
            break;
        case fadeType_t::DECEL:
            return msFadeDecel;
            break;
    }
    return 0;
}



//===============================
//=========== setFade ===========
//===============================
//function for editing or enabling the fading/ramp of the motor control

//set/update fading duration/amount
void controlledMotor::setFade(fadeType_t fadeType, uint32_t msFadeNew){
    //TODO: mutex for msFade variable also used in handle function
    switch(fadeType){
        case fadeType_t::ACCEL:
            ESP_LOGW(TAG, "changed fade-up time from %d to %d", msFadeAccel, msFadeNew);
            msFadeAccel = msFadeNew; 
            break;
        case fadeType_t::DECEL:
            ESP_LOGW(TAG, "changed fade-down time from %d to %d", msFadeDecel, msFadeNew);
            msFadeDecel = msFadeNew;
            break;
    }
}

//enable (set to default value) or disable fading
void controlledMotor::setFade(fadeType_t fadeType, bool enabled){
    uint32_t msFadeNew = 0; //define new fade time as default disabled
    if(enabled){ //enable
        //set to default/configured value
        switch(fadeType){
            case fadeType_t::ACCEL:
                msFadeNew = config.msFadeAccel;
                break;
            case fadeType_t::DECEL:
                msFadeNew = config.msFadeDecel;
                break;
        }
    }
    //apply new Fade value
    setFade(fadeType, msFadeNew); 
}



//==================================
//=========== toggleFade ===========
//==================================
//toggle fading between OFF and default value
bool controlledMotor::toggleFade(fadeType_t fadeType){
    uint32_t msFadeNew = 0;
    bool enabled = false;
    switch(fadeType){
        case fadeType_t::ACCEL:
            if (msFadeAccel == 0){
                msFadeNew = config.msFadeAccel;
                enabled = true;
            } else {
                msFadeNew = 0;
            }
            break;
        case fadeType_t::DECEL:
            if (msFadeDecel == 0){
                msFadeNew = config.msFadeAccel;
                enabled = true;
            } else {
                msFadeNew = 0;
            }
            break;
    }
    //apply new Fade value
    setFade(fadeType, msFadeNew); 

    //return new state (fading enabled/disabled)
    return enabled;
}

