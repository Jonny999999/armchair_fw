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
controlledMotor::controlledMotor(motorSetCommandFunc_t setCommandFunc,  motorctl_config_t config_control, nvs_handle_t * nvsHandle_f, speedSensor * speedSensor_f, controlledMotor ** otherMotor_f):
    //create current sensor
	cSensor(config_control.currentSensor_adc, config_control.currentSensor_ratedCurrent, config_control.currentSnapToZeroThreshold, config_control.currentInverted) {
		//copy parameters for controlling the motor
		config = config_control;
        log = config.loggingEnabled;
		//pointer to update motot dury method
		motorSetCommand = setCommandFunc;
        //pointer to nvs handle
        nvsHandle = nvsHandle_f;
        //pointer to other motor object
        ppOtherMotor = otherMotor_f;
        //pointer to speed sensor
        sSensor = speedSensor_f;

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

    //--- RECEIVE DATA FROM QUEUE ---
    if( xQueueReceive( commandQueue, &commandReceive, timeoutWaitForCommand / portTICK_PERIOD_MS ) ) //wait time is always 0 except when at target duty already
    {
        if(log) ESP_LOGV(TAG, "[%s] Read command from queue: state=%s, duty=%.2f", config.name, motorstateStr[(int)commandReceive.state], commandReceive.duty);
        state = commandReceive.state;
        dutyTarget = commandReceive.duty;
		receiveTimeout = false;
		timestamp_commandReceived = esp_log_timestamp();

    }





// ----- EXPERIMENTAL, DIFFERENT MODES -----
// define target duty differently depending on current contro-mode
//declare variables used inside switch
float ampereNow, ampereTarget, ampereDiff;
float speedDiff;
    switch (mode)
    {
    case motorControlMode_t::DUTY: // regulate to desired duty (as originally)
        //--- convert duty ---
        // define target duty (-100 to 100) from provided duty and motorstate
        // this value is more suitable for t
        // todo scale target input with DUTY-MAX here instead of in joysick cmd generationhe fading algorithm
        switch (commandReceive.state)
        {
        case motorstate_t::BRAKE:
            // update state
            state = motorstate_t::BRAKE;
            // dutyTarget = 0;
            dutyTarget = fabs(commandReceive.duty);
            break;
        case motorstate_t::IDLE:
            dutyTarget = 0;
            break;
        case motorstate_t::FWD:
            dutyTarget = fabs(commandReceive.duty);
            break;
        case motorstate_t::REV:
            dutyTarget = -fabs(commandReceive.duty);
            break;
        }
        break;

#define CURRENT_CONTROL_ALLOWED_AMPERE_DIFF 1 //difference from target where no change is made yet
#define CURRENT_CONTROL_MIN_AMPERE 0.7 //current where motor is turned off
//TODO define different, fixed fading configuration in current mode, fade down can be significantly less (500/500ms fade up worked fine)
    case motorControlMode_t::CURRENT: // regulate to desired current flow
        ampereNow = cSensor.read();
        ampereTarget = config.currentMax * commandReceive.duty / 100; // TODO ensure input data is 0-100 (no duty max), add currentMax to menu/config
        if (commandReceive.state == motorstate_t::REV) ampereTarget = - ampereTarget; //target is negative when driving reverse
        ampereDiff = ampereTarget - ampereNow;
        if(log) ESP_LOGV("TESTING", "[%s] CURRENT-CONTROL: ampereNow=%.2f, ampereTarget=%.2f, diff=%.2f", config.name, ampereNow, ampereTarget, ampereDiff); // todo handle brake

        //--- when IDLE to keep the current at target zero motor needs to be on for some duty (to compensate generator current) 
        if (commandReceive.duty == 0 && fabs(ampereNow) < CURRENT_CONTROL_MIN_AMPERE){ //stop motors completely when current is very low already
            dutyTarget = 0;
        }
        else if (fabs(ampereDiff) > CURRENT_CONTROL_ALLOWED_AMPERE_DIFF || commandReceive.duty == 0) //#### BOOST BY 1 A
        {
            if (ampereDiff > 0 && commandReceive.state != motorstate_t::REV) // forward need to increase current
            {
                dutyTarget = 100; // todo add custom fading depending on diff? currently very dependent of fade times
            }
            else if (ampereDiff < 0 && commandReceive.state != motorstate_t::FWD) // backward need to increase current (more negative)
            {
                dutyTarget = -100;
            }
            else // fwd too much, rev too much -> decrease
            {
                dutyTarget = 0;
            }
            if(log) ESP_LOGV("TESTING", "[%s] CURRENT-CONTROL: set target to %.0f%%", config.name, dutyTarget);
        }
        else
        {
            dutyTarget = dutyNow; // target current reached
            if(log) ESP_LOGD("TESTING", "[%s] CURRENT-CONTROL: target current %.3f reached", config.name, dutyTarget);
        }
        break;

#define SPEED_CONTROL_MAX_SPEED_KMH 10
#define SPEED_CONTROL_ALLOWED_KMH_DIFF 0.6
#define SPEED_CONTROL_MIN_SPEED 0.7 //" start from standstill" always accelerate to this speed, ignoring speedsensor data
    case motorControlMode_t::SPEED: // regulate to desired speed
        speedNow = sSensor->getKmph();
    
        //caculate target speed from input
        speedTarget = SPEED_CONTROL_MAX_SPEED_KMH * commandReceive.duty / 100; // TODO add maxSpeed to config
        // target speed negative when driving reverse
        if (commandReceive.state == motorstate_t::REV)
            speedTarget = -speedTarget;
    if (sSensor->getTimeLastUpdate() != timestamp_speedLastUpdate ){ //only modify duty when new speed data available
        timestamp_speedLastUpdate = sSensor->getTimeLastUpdate(); //TODO get time only once
        speedDiff = speedTarget - speedNow;
    } else {
        if(log) ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: no new speed data, not changing duty", config.name);
        speedDiff = 0;
    }
        if(log) ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: target-speed=%.2f, current-speed=%.2f, diff=%.3f", config.name, speedTarget, speedNow, speedDiff);

        //stop when target is 0
        if (commandReceive.duty == 0) { //TODO add IDLE, BRAKE state
        if(log) ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: OFF, target is 0... current-speed=%.2f, diff=%.3f", config.name, speedNow, speedDiff);
            dutyTarget = 0;
        }
        else if (fabs(speedNow) < SPEED_CONTROL_MIN_SPEED){ //start from standstill or too slow (not enough speedsensor data)
            if (log)
                ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: starting from standstill -> increase duty... target-speed=%.2f, current-speed=%.2f, diff=%.3f", config.name, speedTarget, speedNow, speedDiff);
            if (commandReceive.state == motorstate_t::FWD)
            dutyTarget = 100;
            else if (commandReceive.state == motorstate_t::REV)
            dutyTarget = -100;
        }
        else if (fabs(speedDiff) > SPEED_CONTROL_ALLOWED_KMH_DIFF) //speed too fast/slow
        {
            if (speedDiff > 0 && commandReceive.state != motorstate_t::REV) // forward need to increase speed
            {
                // TODO retain max duty here
                dutyTarget = 100; // todo add custom fading depending on diff? currently very dependent of fade times
            if(log) ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: speed to low (fwd), diff=%.2f, increasing set target from %.1f%% to %.1f%%", config.name, speedDiff, dutyNow, dutyTarget);
            }
            else if (speedDiff < 0 && commandReceive.state != motorstate_t::FWD) // backward need to increase speed (more negative)
            {
                dutyTarget = -100;
            if(log) ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: speed to low (rev), diff=%.2f, increasing set target from %.1f%% to %.1f%%", config.name, speedDiff, dutyNow, dutyTarget);
            }
            else // fwd too much, rev too much -> decrease
            {
                dutyTarget = 0;
            if(log) ESP_LOGV("TESTING", "[%s] SPEED-CONTROL: speed to high, diff=%.2f, decreasing set target from %.1f%% to %.1f%%", config.name, speedDiff, dutyNow, dutyTarget);
            }
        }
        else
        {
            dutyTarget = dutyNow; // target speed reached
            if(log) ESP_LOGD("TESTING", "[%s] SPEED-CONTROL: target speed %.3f reached", config.name, speedTarget);
        }

        break;
    }




//--- TIMEOUT NO DATA ---
// turn motors off if no data received for a long time (e.g. no uart data or control task offline)
if ( dutyNow != 0 && esp_log_timestamp() - timestamp_commandReceived > TIMEOUT_IDLE_WHEN_NO_COMMAND && !receiveTimeout)
{
    if(log) ESP_LOGE(TAG, "[%s] TIMEOUT, motor active, but no target data received for more than %ds -> switch from duty=%.2f to IDLE", config.name, TIMEOUT_IDLE_WHEN_NO_COMMAND / 1000, dutyTarget);
    receiveTimeout = true;
    state = motorstate_t::IDLE;
    dutyTarget = 0; // todo put this in else section of queue (no data received) and add control mode "timeout"?
}


    //--- CALCULATE DUTY-DIFF ---
        dutyDelta = dutyTarget - dutyNow;
    //positive: need to increase by that value
    //negative: need to decrease


    //--- DETECT ALREADY AT TARGET ---
    // when already at exact target duty there is no need to run very fast to handle fading
    //-> slow down loop by waiting significantly longer for new commands to arrive
    if (mode != motorControlMode_t::CURRENT  //dont slow down when in CURRENT mode at all
    && ((dutyDelta == 0 && !config.currentLimitEnabled && !config.tractionControlSystemEnabled && mode != motorControlMode_t::SPEED) //when neither of current-limit, tractioncontrol or speed-mode is enabled slow down when target reached 
    || (dutyTarget == 0 && dutyNow == 0))) //otherwise only slow down when when actually off
    {
        //increase queue timeout when duty is the same (once)
        if (timeoutWaitForCommand == 0)
        { // TODO verify if state matches too?
            if(log) ESP_LOGI(TAG, "[%s] already at target duty %.2f, slowing down...", config.name, dutyTarget);
            timeoutWaitForCommand = TIMEOUT_QUEUE_WHEN_AT_TARGET; // wait in queue very long, for new command to arrive
        }
        vTaskDelay(20 / portTICK_PERIOD_MS); // add small additional delay overall, in case the same commands get spammed
    }
    //reset timeout when duty differs again (once)
    else if (timeoutWaitForCommand != 0)
    {
        timeoutWaitForCommand = 0; // dont wait additional time for new commands, handle fading fast
        if(log) ESP_LOGI(TAG, "[%s] duty changed to %.2f, resuming at full speed", config.name, dutyTarget);
        // adjust lastRun timestamp to not mess up fading, due to much time passed but with no actual duty change
        timestampLastRunUs = esp_timer_get_time() - 20*1000; //subtract approx 1 cycle delay
    }
    //TODO skip rest of the handle function below using return? Some regular driver updates sound useful though


	//--- BRAKE ---
	//brake immediately, update state, duty and exit this cycle of handle function
	if (state == motorstate_t::BRAKE){
		if(log) ESP_LOGD(TAG, "braking - skip fading");
		motorSetCommand({motorstate_t::BRAKE, dutyTarget});
		if(log) ESP_LOGD(TAG, "[%s] Set Motordriver: state=%s, duty=%.2f - Measurements: current=%.2f, speed=N/A", config.name, motorstateStr[(int)state], dutyNow, currentNow);
		//dutyNow = 0;
		return; //no need to run the fade algorithm
	}


	//----- FADING -----
    //calculate passed time since last run
    int64_t usPassed = esp_timer_get_time() - timestampLastRunUs;

    //--- calculate increment ---
    //calculate increment for fading UP with passed time since last run and configured fade time
    if (tcs_isExceeded) // disable acceleration when slippage is currently detected
        dutyIncrementAccel = 0;
    else if (msFadeAccel > 0)
        dutyIncrementAccel = (usPassed / ((float)msFadeAccel * 1000)) * 100; // TODO define maximum increment - first run after startup (or long) pause can cause a very large increment
    else //no accel limit (immediately set to 100)
        dutyIncrementAccel = 100;

    //calculate increment for fading DOWN with passed time since last run and configured fade time
    if (msFadeDecel > 0)
        dutyIncrementDecel = ( usPassed / ((float)msFadeDecel * 1000) ) * 100; 
    else //no decel limit (immediately reduce to 0)
        dutyIncrementDecel = 100;
    
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
			if(log) ESP_LOGW(TAG, "[%s] current limit exceeded! now=%.3fA max=%.1fA => decreased duty from %.3f to %.3f", config.name, currentNow, config.currentMax, dutyOld, dutyNow);
		}
	}


    //----- TRACTION CONTROL -----
    //reduce duty when turning faster than expected
    //TODO only run this when speed sensors actually updated
    //handle tcs when enabled and new speed sensor data is available  TODO: currently assumes here that speed sensor data of other motor updated as well
    #define TCS_MAX_ALLOWED_RATIO_DIFF 0.1 //when motor speed ratio differs more than that, one motor is slowed down
    #define TCS_NO_SPEED_DATA_TIMEOUT_US 200*1000
    #define TCS_MIN_SPEED_KMH 1 //must be at least that fast for TCS to be enabled
    //TODO rework this: clearer structure (less nested if statements)
    if (config.tractionControlSystemEnabled && mode == motorControlMode_t::SPEED && sSensor->getTimeLastUpdate() != tcs_timestampLastSpeedUpdate && (esp_timer_get_time() - tcs_timestampLastRun < TCS_NO_SPEED_DATA_TIMEOUT_US)){
        //update last speed update received
        tcs_timestampLastSpeedUpdate = sSensor->getTimeLastUpdate(); //TODO: re-use tcs_timestampLastRun in if statement, instead of having additional variable SpeedUpdate

        //calculate time passed since last run
        uint32_t tcs_usPassed = esp_timer_get_time() - tcs_timestampLastRun; // passed time since last time handled
        tcs_timestampLastRun = esp_timer_get_time();

        //get motor stats
        float speedNowThis = sSensor->getKmph();
        float speedNowOther = (*ppOtherMotor)->getCurrentSpeed();
        float speedTargetThis = speedTarget;
        float speedTargetOther = (*ppOtherMotor)->getTargetSpeed();
        float dutyTargetOther = (*ppOtherMotor)->getTargetDuty();
        float dutyTargetThis = dutyTarget;
        float dutyNowOther = (*ppOtherMotor)->getDuty();
        float dutyNowThis = dutyNow;


        //calculate expected ratio
        float ratioSpeedTarget = speedTargetThis / speedTargetOther;
        //calculate current ratio of actual measured rotational speed
        float ratioSpeedNow = speedNowThis / speedNowOther;
        //calculate current duty ration (logging only)
        float ratioDutyNow = dutyNowThis / dutyNowOther;

        //calculate unexpected difference
        float ratioDiff = ratioSpeedNow - ratioSpeedTarget;
        if(log) ESP_LOGD("TESTING", "[%s] TCS: speedThis=%.3f, speedOther=%.3f, ratioSpeedTarget=%.3f, ratioSpeedNow=%.3f, ratioDutyNow=%.3f, diff=%.3f", config.name, speedNowThis, speedNowOther, ratioSpeedTarget, ratioSpeedNow, ratioDutyNow, ratioDiff);

        //-- handle rotating faster than expected --
        //TODO also increase duty when other motor is slipping? (diff negative)
        if (speedNowThis < TCS_MIN_SPEED_KMH) { //disable / turn off TCS when currently too slow (danger of deadlock)
            tcs_isExceeded = false;
            tcs_usExceeded = 0;
        }
        else if (ratioDiff > TCS_MAX_ALLOWED_RATIO_DIFF ) // motor turns too fast compared to expected target ratio
        {
            if (!tcs_isExceeded) // just started being too fast
            {
                tcs_timestampBeginExceeded = esp_timer_get_time();
                tcs_isExceeded = true; //also blocks further acceleration (fade)
                if(log) ESP_LOGW("TESTING", "[%s] TCS: now exceeding max allowed ratio diff! diff=%.2f max=%.2f", config.name, ratioDiff, TCS_MAX_ALLOWED_RATIO_DIFF);
            }
            else
            { // too fast for more than 2 cycles already
                tcs_usExceeded = esp_timer_get_time() - tcs_timestampBeginExceeded; //time too fast already
                if(log) ESP_LOGI("TESTING", "[%s] TCS: faster than expected since %dms, current ratioDiff=%.2f  -> slowing down", config.name, tcs_usExceeded/1000, ratioDiff);
                // calculate amount duty gets decreased
                float dutyDecrement = (tcs_usPassed / ((float)msFadeDecel * 1000)) * 100; //TODO optimize dynamic increment: P:scale with ratio-difference, I: scale with duration exceeded
                // decrease duty
                if(log) ESP_LOGI("TESTING", "[%s] TCS: msPassed=%.3f, reducing duty by %.3f%%", config.name, (float)tcs_usPassed/1000, dutyDecrement);
                fade(&dutyNow, 0, -dutyDecrement); //reduce duty but not less than 0
            }
        }
        else
        { // not exceeded
            tcs_isExceeded = false;
            tcs_usExceeded = 0;
        }
    }
    else // TCS mode not active or timed out
    {    // not exceeded
        tcs_isExceeded = false;
        tcs_usExceeded = 0;
        }

	


    //--- define new motorstate --- (-100 to 100 => direction)
	state=getStateFromDuty(dutyNow);


	//--- DEAD TIME ----
	//ensure minimum idle time between direction change to prevent driver overload
	//FWD -> IDLE -> FWD  continue without waiting
	//FWD -> IDLE -> REV  wait for dead-time in IDLE
	//TODO check when changed only?
    if (config.deadTimeMs > 0) { //deadTime is enabled
	    if (	//not enough time between last direction state
	    		(   state == motorstate_t::FWD && (esp_log_timestamp() - timestampsModeLastActive[(int)motorstate_t::REV] < config.deadTimeMs))
	    		|| (state == motorstate_t::REV && (esp_log_timestamp() - timestampsModeLastActive[(int)motorstate_t::FWD] < config.deadTimeMs))
	       ){
	    	if(log) ESP_LOGD(TAG, "waiting dead-time... dir change %s -> %s", motorstateStr[(int)statePrev], motorstateStr[(int)state]);
	    	if (!deadTimeWaiting){ //log start
	    		deadTimeWaiting = true;
	    		if(log) ESP_LOGI(TAG, "starting dead-time... %s -> %s", motorstateStr[(int)statePrev], motorstateStr[(int)state]);
	    	}
	    	//force IDLE state during wait
	    	state = motorstate_t::IDLE;
	    	dutyNow = 0;
	    } else {
	    	if (deadTimeWaiting){ //log end
	    		deadTimeWaiting = false;
	    		if(log) ESP_LOGI(TAG, "dead-time ended - continue with %s", motorstateStr[(int)state]);
	    	}
	    	if(log) ESP_LOGV(TAG, "deadtime: no change below deadtime detected... dir=%s, duty=%.1f", motorstateStr[(int)state], dutyNow);
	    }
    }
			

	//--- save current actual motorstate and timestamp ---
	//needed for deadtime
	timestampsModeLastActive[(int)getStateFromDuty(dutyNow)] = esp_log_timestamp();
	//(-100 to 100 => direction)
	statePrev = getStateFromDuty(dutyNow);


    //--- apply new target to motor ---
    motorSetCommand({state, (float)fabs(dutyNow)});
	if(log) ESP_LOGI(TAG, "[%s] Set Motordriver: state=%s, duty=%.2f - Measurements: current=%.2f, speed=N/A", config.name, motorstateStr[(int)state], dutyNow, currentNow);
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
    if(log) ESP_LOGI(TAG, "[%s] setTarget: Inserting command to queue: state='%s'(%d), duty=%.2f", config.name, motorstateStr[(int)commandSend.state], (int)commandSend.state, commandSend.duty);
    //send command to queue (overwrite if an old command is still in the queue and not processed)
    xQueueOverwrite( commandQueue, ( void * )&commandSend);
    //xQueueSend( commandQueue, ( void * )&commandSend, ( TickType_t ) 0 );
    if(log) ESP_LOGD(TAG, "finished inserting new command");
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