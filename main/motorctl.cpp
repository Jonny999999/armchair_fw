#include "motorctl.hpp"

//tag for logging
static const char * TAG = "motor-control";


//=============================
//======== constructor ========
//=============================
//constructor, simultaniously initialize instance of motor driver 'motor' with provided config (see below line after ':')
controlledMotor::controlledMotor(single100a_config_t config_driver,  motorctl_config_t config_control): motor(config_driver) {
    //copy parameters for controlling the motor
    config = config_control;

    init();
    //TODO: add currentsensor object here
}



//============================
//========== init ============
//============================
void controlledMotor::init(){
    commandQueue = xQueueCreate( 1, sizeof( struct motorCommand_t ) );
}



//==============================
//=========== handle ===========
//==============================
//function that controls the motor driver and handles fading/ramp and current limit
void controlledMotor::handle(){

    //TODO: current sensor
    //TODO: delay when switching direction?
    //TODO: History: skip fading when motor was running fast recently

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
    dutyIncrementUp = ( usPassed / ((float)config.msFadeUp * 1000) ) * 100; //TODO define maximum increment - first run after startup (or long) pause can cause a very large increment

    //calculate increment for fading DOWN with passed time since last run and configured fade time
    if (config.msFadeDown > 0){
        dutyIncrementDown = ( usPassed / ((float)config.msFadeDown * 1000) ) * 100; 
    } else {
        dutyIncrementDown = 100;
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

    //--- fade up ---
    //dutyDelta is higher than IncrementUp -> fade up
    if(dutyDelta > dutyIncrementUp){
        ESP_LOGV(TAG, "*fading up*: target=%.2f%% - previous=%.2f%% - increment=%.6f%% - usSinceLastRun=%d", dutyTarget, dutyNow, dutyIncrementUp, (int)usPassed);
        dutyNow += dutyIncrementUp; //increase duty by increment
    }

    //--- fade down ---
    //dutyDelta is more negative than -IncrementDown -> fade down
    else if (dutyDelta < -dutyIncrementDown){
        ESP_LOGV(TAG, "*fading down*: target=%.2f%% - previous=%.2f%% - increment=%.6f%% - usSinceLastRun=%d", dutyTarget, dutyNow, dutyIncrementDown, (int)usPassed);
        dutyNow -= dutyIncrementDown; //decrease duty by increment
    }

    //--- set to target ---
    //duty is already very close to target (closer than IncrementUp or IncrementDown)
    else{ 
        //immediately set to target duty
        dutyNow = dutyTarget;
    }
    

    //define motorstate from converted duty -100 to 100
    //apply target duty and state to motor driver
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

    //--- apply to motor ---
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

