#include "motorctl.hpp"

//tag for logging
static const char * TAG = "motor-control";


//=============================
//======== constructor ========
//=============================
//constructor, simultaniously initialize instance of motor driver 'motor' and current sensor 'cSensor' with provided config (see below lines after ':')
controlledMotor::controlledMotor(single100a_config_t config_driver,  motorctl_config_t config_control): 
	motor(config_driver), 
	cSensor(config_control.currentSensor_adc, config_control.currentSensor_ratedCurrent) {
		//copy parameters for controlling the motor
		config = config_control;
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



//==============================
//=========== handle ===========
//==============================
//function that controls the motor driver and handles fading/ramp and current limit
void controlledMotor::handle(){

    //TODO: delay when switching direction?
    //TODO: History: skip fading when motor was running fast recently / alternatively add rot-speed sensor

    //--- receive commands from queue ---
    if( xQueueReceive( commandQueue, &commandReceive, ( TickType_t ) 0 ) )
    {
        ESP_LOGD(TAG, "Read command from queue: state=%s, duty=%.2f", motorstateStr[(int)commandReceive.state], commandReceive.duty);
        state = commandReceive.state;
        dutyTarget = commandReceive.duty;

        //--- convert duty ---
        //define target duty (-100 to 100) from provided duty and motorstate
        //this value is more suitable for the fading algorithm
        switch(commandReceive.state){
            case motorstate_t::BRAKE:
                //update state
                state = motorstate_t::BRAKE;
                dutyTarget = 0;
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
                motor.set(motorstate_t::BRAKE, 0);
                dutyNow = 0;
                return; //no need to run the fade algorithm
    }


    //--- calculate difference ---
    dutyDelta = dutyTarget - dutyNow;
    //positive: need to increase by that value
    //negative: need to decrease


	//----- fading -----
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


	//----- current limit -----
	if ((config.currentLimitEnabled) && (dutyDelta != 0)){
		currentNow = cSensor.read();
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


    //--- define motorstate ---
	//from converted duty -100 to 100
    //forward
    if(dutyNow > 0){
        state = motorstate_t::FWD;
    }
    //reverse
    else if (dutyNow < 0){
        state = motorstate_t::REV;
    }
    //idle
    else {
        state = motorstate_t::IDLE;
    }

    //--- apply new target to motor ---
    motor.set(state, fabs(dutyNow));
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

    ESP_LOGD(TAG, "Inserted command to queue: state=%s, duty=%.2f", motorstateStr[(int)commandSend.state], commandSend.duty);
    //send command to queue (overwrite if an old command is still in the queue and not processed)
    xQueueOverwrite( commandQueue, ( void * )&commandSend);
    //xQueueSend( commandQueue, ( void * )&commandSend, ( TickType_t ) 0 );

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
//=========== setFade ===========
//===============================
//function for editing or enabling the fading/ramp of the motor control

//set/update fading duration/amount
void controlledMotor::setFade(fadeType_t fadeType, uint32_t msFadeNew){
    //TODO: mutex for msFade variable also used in handle function
    switch(fadeType){
        case fadeType_t::ACCEL:
            msFadeAccel = msFadeNew; 
            break;
        case fadeType_t::DECEL:
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

