#include "motorctl.hpp"
#include "esp_log.h"
#include "types.hpp"

//tag for logging
static const char * TAG = "motor-control";

#define TIMEOUT_IDLE_WHEN_NO_COMMAND 15000 // turn motor off when still on and no new command received within that time
#define TIMEOUT_QUEUE_WHEN_AT_TARGET 5000  // time waited for new command when motors at target duty (e.g. IDLE) (no need to handle fading in fast loop)

//====================================
//========== motorctl task ===========
//====================================
//task for handling the motors (ramp, current limit, driver)
void task_motorctl( void * ptrControlledMotor ){
    //get pointer to controlledMotor instance from task parameter
    controlledMotor * motor = (controlledMotor *)ptrControlledMotor;
    ESP_LOGW(TAG, "Task-motorctl [%s]: starting handle loop...", motor->getName());
    while(1){
        motor->handle();
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}



//=============================
//======== constructor ========
//=============================
//constructor, simultaniously initialize instance of motor driver 'motor' and current sensor 'cSensor' with provided config (see below lines after ':')
controlledMotor::controlledMotor(motorSetCommandFunc_t setCommandFunc,  motorctl_config_t config_control, nvs_handle_t * nvsHandle_f): 
	cSensor(config_control.currentSensor_adc, config_control.currentSensor_ratedCurrent, config_control.currentInverted) {
		//copy parameters for controlling the motor
		config = config_control;
		//pointer to update motot dury method
		motorSetCommand = setCommandFunc;
        //pointer to nvs handle
        nvsHandle = nvsHandle_f;

        //create queue, initialize config values
		init();
	}



//============================
//========== init ============
//============================
void controlledMotor::init(){
    commandQueue = xQueueCreate( 1, sizeof( struct motorCommand_t ) );
    if (commandQueue == NULL)
    ESP_LOGE(TAG, "Failed to create command-queue");
    else
    ESP_LOGI(TAG, "[%s] Initialized command-queue", config.name);

    // load config values from nvs, otherwise use default from config object
    loadAccelDuration();
    loadDecelDuration();

    // turn motor off initially
    motorSetCommand({motorstate_t::IDLE, 0.00});

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
    if( xQueueReceive( commandQueue, &commandReceive, timeoutWaitForCommand / portTICK_PERIOD_MS ) ) //wait time is always 0 except when at target duty already
    {
        ESP_LOGV(TAG, "[%s] Read command from queue: state=%s, duty=%.2f", config.name, motorstateStr[(int)commandReceive.state], commandReceive.duty);
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
    // turn motors off if no data received for a long time (e.g. no uart data or control task offline)
    if (dutyNow != 0 && esp_log_timestamp() - timestamp_commandReceived > TIMEOUT_IDLE_WHEN_NO_COMMAND && !receiveTimeout)
    {
        ESP_LOGE(TAG, "[%s] TIMEOUT, motor active, but no target data received for more than %ds -> switch from duty=%.2f to IDLE", config.name, TIMEOUT_IDLE_WHEN_NO_COMMAND / 1000, dutyTarget);
        receiveTimeout = true;
        state = motorstate_t::IDLE;
        dutyTarget = 0;
    }

    //--- calculate difference ---
    dutyDelta = dutyTarget - dutyNow;
    //positive: need to increase by that value
    //negative: need to decrease

    //--- already at target ---
    // when already at exact target duty there is no need to run very fast to handle fading
    //-> slow down loop by waiting significantly longer for new commands to arrive
    if ((dutyDelta == 0 && !config.currentLimitEnabled) || (dutyTarget == 0 && dutyNow == 0)) //when current limit enabled only slow down when duty is 0
    {
        //increase timeout once when duty is the same (once)
        if (timeoutWaitForCommand == 0)
        { // TODO verify if state matches too?
            ESP_LOGI(TAG, "[%s] already at target duty %.2f, slowing down...", config.name, dutyTarget);
            timeoutWaitForCommand = TIMEOUT_QUEUE_WHEN_AT_TARGET; // wait in queue very long, for new command to arrive
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // add small additional delay overall, in case the same commands get spammed
    }
    //reset timeout when duty differs again (once)
    else if (timeoutWaitForCommand != 0)
    {
        timeoutWaitForCommand = 0; // dont wait additional time for new commands, handle fading fast
        ESP_LOGI(TAG, "[%s] duty changed to %.2f, resuming at full speed", config.name, dutyTarget);
        // adjust lastRun timestamp to not mess up fading, due to much time passed but with no actual duty change
        timestampLastRunUs = esp_timer_get_time() - 20*1000; //subtract approx 1 cycle delay
    }
    //TODO skip rest of the handle function below using return? Some regular driver updates sound useful though


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
		ESP_LOGD(TAG, "[%s] Set Motordriver: state=%s, duty=%.2f - Measurements: current=%.2f, speed=N/A", config.name, motorstateStr[(int)state], dutyNow, currentNow);
		//dutyNow = 0;
		return; //no need to run the fade algorithm
	}




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
			ESP_LOGW(TAG, "[%s] current limit exceeded! now=%.3fA max=%.1fA => decreased duty from %.3f to %.3f", config.name, currentNow, config.currentMax, dutyOld, dutyNow);
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
			ESP_LOGI(TAG, "starting dead-time... %s -> %s", motorstateStr[(int)statePrev], motorstateStr[(int)state]);
		}
		//force IDLE state during wait
		state = motorstate_t::IDLE;
		dutyNow = 0;
	} else {
		if (deadTimeWaiting){ //log end
			deadTimeWaiting = false;
			ESP_LOGI(TAG, "dead-time ended - continue with %s", motorstateStr[(int)state]);
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
	ESP_LOGI(TAG, "[%s] Set Motordriver: state=%s, duty=%.2f - Measurements: current=%.2f, speed=N/A", config.name, motorstateStr[(int)state], dutyNow, currentNow);
    //note: BRAKE state is handled earlier
    

    //--- update timestamp ---
    timestampLastRunUs = esp_timer_get_time(); //update timestamp last run with current timestamp in microseconds
}



//===============================
//========== setTarget ==========
//===============================
//function to set the target mode and duty of a motor
//puts the provided command in a queue for the handle function running in another task
void controlledMotor::setTarget(motorCommand_t commandSend){
    ESP_LOGI(TAG, "[%s] setTarget: Inserting command to queue: state='%s'(%d), duty=%.2f", config.name, motorstateStr[(int)commandSend.state], (int)commandSend.state, commandSend.duty);
    //send command to queue (overwrite if an old command is still in the queue and not processed)
    xQueueOverwrite( commandQueue, ( void * )&commandSend);
    //xQueueSend( commandQueue, ( void * )&commandSend, ( TickType_t ) 0 );
    ESP_LOGD(TAG, "finished inserting new command");
}

// accept target state and duty as separate agrguments:
void controlledMotor::setTarget(motorstate_t state_f, float duty_f){
    // create motorCommand struct from the separate parameters, and run the method to insert that new command
    setTarget({state_f, duty_f});
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

//==============================
//======= getFadeDefault =======
//==============================
//return default accel / decel time (from config)
uint32_t controlledMotor::getFadeDefault(fadeType_t fadeType){
    switch(fadeType){
        case fadeType_t::ACCEL:
            return config.msFadeAccel;
            break;
        case fadeType_t::DECEL:
            return config.msFadeDecel;
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
            ESP_LOGW(TAG, "[%s] changed fade-up time from %d to %d", config.name, msFadeAccel, msFadeNew);
            writeAccelDuration(msFadeNew);
            break;
        case fadeType_t::DECEL:
            ESP_LOGW(TAG, "[%s] changed fade-down time from %d to %d",config.name, msFadeDecel, msFadeNew);
            // write new value to nvs and update the variable
            writeDecelDuration(msFadeNew);
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




//-----------------------------
//----- loadAccelDuration -----
//-----------------------------
// load stored value from nvs if not successfull uses config default value
void controlledMotor::loadAccelDuration(void)
{
    // load default value
    msFadeAccel = config.msFadeAccel;
    // read from nvs
    uint32_t valueNew;
    char key[15];
    snprintf(key, 15, "m-%s-accel", config.name);
    esp_err_t err = nvs_get_u32(*nvsHandle, key, &valueNew);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGW(TAG, "Successfully read value '%s' from nvs. Overriding default value %d with %d", key, config.msFadeAccel, valueNew);
        msFadeAccel = valueNew;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "nvs: the value '%s' is not initialized yet, keeping default value %d", key, msFadeAccel);
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading nvs!", esp_err_to_name(err));
    }
}

//-----------------------------
//----- loadDecelDuration -----
//-----------------------------
void controlledMotor::loadDecelDuration(void)
{
    // load default value
    msFadeDecel = config.msFadeDecel;
    // read from nvs
    uint32_t valueNew;
    char key[15];
    snprintf(key, 15, "m-%s-decel", config.name);
    esp_err_t err = nvs_get_u32(*nvsHandle, key, &valueNew);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGW(TAG, "Successfully read value '%s' from nvs. Overriding default value %d with %d", key, config.msFadeDecel, valueNew);
        msFadeDecel = valueNew;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "nvs: the value '%s' is not initialized yet, keeping default value %d", key, msFadeDecel);
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading nvs!", esp_err_to_name(err));
    }
}




//------------------------------
//----- writeAccelDuration -----
//------------------------------
// write provided value to nvs to be persistent and update the local variable msFadeAccel
void controlledMotor::writeAccelDuration(uint32_t newValue)
{
    // check if unchanged
    if(msFadeAccel == newValue){
        ESP_LOGW(TAG, "value unchanged at %d, not writing to nvs", newValue);
        return;
    }
    // generate nvs storage key
    char key[15];
    snprintf(key, 15, "m-%s-accel", config.name);
    // update nvs value
    ESP_LOGW(TAG, "[%s] updating nvs value '%s' from %d to %d", config.name, key, msFadeAccel, newValue);
    esp_err_t err = nvs_set_u32(*nvsHandle, key, newValue);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed writing");
    err = nvs_commit(*nvsHandle);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed committing updates");
    else
        ESP_LOGI(TAG, "nvs: successfully committed updates");
    // update variable
    msFadeAccel = newValue;
}

//------------------------------
//----- writeDecelDuration -----
//------------------------------
// write provided value to nvs to be persistent and update the local variable msFadeDecel
// TODO: reduce duplicate code
void controlledMotor::writeDecelDuration(uint32_t newValue)
{
    // check if unchanged
    if(msFadeDecel == newValue){
        ESP_LOGW(TAG, "value unchanged at %d, not writing to nvs", newValue);
        return;
    }
    // generate nvs storage key
    char key[15];
    snprintf(key, 15, "m-%s-decel", config.name);
    // update nvs value
    ESP_LOGW(TAG, "[%s] updating nvs value '%s' from %d to %d", config.name, key, msFadeDecel, newValue);
    esp_err_t err = nvs_set_u32(*nvsHandle, key, newValue);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed writing");
    err = nvs_commit(*nvsHandle);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed committing updates");
    else
        ESP_LOGI(TAG, "nvs: successfully committed updates");
    // update variable
    msFadeDecel = newValue;
}