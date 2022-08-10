#include "auto.hpp"

//tag for logging
static const char * TAG = "automatedArmchair";


//=============================
//======== constructor ========
//=============================
automatedArmchair::automatedArmchair(void) {
    //create command queue
    commandQueue = xQueueCreate( 32, sizeof( commandSimple_t ) ); //TODO add max size to config?
}



//==============================
//====== generateCommands ======
//==============================
motorCommands_t automatedArmchair::generateCommands() {
    //check if previous command is finished
    if ( esp_log_timestamp() > timestampCmdFinished ) {
        //get next command from queue
        if( xQueueReceive( commandQueue, &cmdCurrent, pdMS_TO_TICKS(0) ) ) {
            ESP_LOGI(TAG, "running next command from queue...");
            //calculate timestamp the command is finished
            timestampCmdFinished = esp_log_timestamp() + cmdCurrent.msDuration;
            //copy the new commands
            motorCommands = cmdCurrent.commands;
        } else { //queue empty
            ESP_LOGD(TAG, "no new command in queue -> set motors to IDLE");
            motorCommands = cmds_bothMotorsIdle;
        }
    } else { //previous command still running
        ESP_LOGD(TAG, "command still running -> no change");
    }

    return motorCommands;
}



//============================
//======== addCommand ========
//============================
//function that adds a basic command to the queue
void automatedArmchair::addCommand(commandSimple_t command) {
    //add command to queue
     if ( xQueueSend( commandQueue, ( void * )&command, ( TickType_t ) 0 ) ){
         ESP_LOGI(TAG, "Successfully inserted command to queue");
     } else {
         ESP_LOGE(TAG, "Failed to insert new command to queue");
     }
}



//===============================
//======== clearCommands ========
//===============================
//function that deletes all pending/queued commands
//e.g. when switching modes
motorCommands_t automatedArmchair::clearCommands() {
    //clear command queue
    xQueueReset( commandQueue );
    ESP_LOGW(TAG, "command queue was successfully emptied");
    //return commands for idling both motors
    return cmds_bothMotorsIdle;
}

