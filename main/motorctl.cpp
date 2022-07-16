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


    //--- calculate difference ---
    dutyDelta = dutyTarget - dutyNow;
    //positive: need to increase by that value
    //negative: need to decrease

    //--- fade up ---
    if(dutyDelta > dutyIncrementUp){ //target duty his higher than current duty -> fade up
        ESP_LOGV(TAG, "*fading up*: target=%.2f%% - previous=%.2f%% - increment=%.6f%% - usSinceLastRun=%d", dutyTarget, dutyNow, dutyIncrementUp, (int)usPassed);
        dutyNow += dutyIncrementUp; //increase duty by increment

    //--- set lower ---
    //} else { //target duty is lower than current duty -> immediately set to target
    //    ESP_LOGV(TAG, "*setting to target*: target=%.2f%% - previous=%.2f%% ", dutyTarget, dutyNow);
    //    dutyNow = dutyTarget; //set target duty
    //}
    
    //--- fade down ---
    } else { //target duty is lower than current duty -> fade down
        ESP_LOGV(TAG, "*fading up*: target=%.2f%% - previous=%.2f%% - increment=%.6f%% - usSinceLastRun=%d", dutyTarget, dutyNow, dutyIncrementDown, (int)usPassed);
        if (dutyTarget < dutyIncrementDown){ //immediately set to target when closer than increment (avoid negative duty)
            dutyNow = dutyTarget;
        } else {
            dutyNow -= dutyIncrementDown; //decrease duty by increment
        }
    }

    //--- apply to motor ---
    //apply target duty and state to motor driver
    motor.set(state, dutyNow);

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

