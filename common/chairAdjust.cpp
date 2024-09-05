extern "C"
{
#include "esp_log.h"
#include <string.h>
}

#include "chairAdjust.hpp"



//--- gloabl variables ---
// strings for logging the rest state
const char* restStateStr[] = {"REST_OFF", "REST_DOWN", "REST_UP"};

//--- local variables ---
//tag for logging
static const char * TAG = "chair-adjustment";



//=============================
//======== constructor ========
//=============================
cControlledRest::cControlledRest(gpio_num_t gpio_up_f, gpio_num_t gpio_down_f, uint32_t travelDurationMs, const char * name_f, float defaultPosition):travelDuration(travelDurationMs){
    strcpy(name, name_f);
    gpio_up = gpio_up_f;
    gpio_down = gpio_down_f;
    positionNow = defaultPosition;
    positionTarget = positionNow;
    init();
}



//====================
//======= init =======
//====================
// init gpio pins for relays
void cControlledRest::init()
{
    ESP_LOGW(TAG, "[%s] initializing gpio pins %d, %d for relays...", name, gpio_up, gpio_down);
    // configure 2 gpio pins
    gpio_pad_select_gpio(gpio_up);
    gpio_set_direction(gpio_up, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(gpio_down);
    gpio_set_direction(gpio_down, GPIO_MODE_OUTPUT);
    // both relays off initially
    gpio_set_level(gpio_down, 0);
    gpio_set_level(gpio_up, 0);
    state = REST_OFF;
}



//==========================
//===== updatePosition =====
//==========================
// calculate and update position in percent based of time running in current direction
void cControlledRest::updatePosition(){
        uint32_t now = esp_log_timestamp();
        uint32_t timeRan = now - timestamp_lastPosUpdate;
        timestamp_lastPosUpdate = now;
        float positionOld = positionNow;

        // calculate new percentage
        switch (state)
        {
        case REST_UP:
            positionNow += (float)timeRan / travelDuration * 100;
            break;
        case REST_DOWN:
            positionNow -= (float)timeRan / travelDuration * 100;
            break;
        case REST_OFF:
            //no change
            ESP_LOGW(TAG, "updatePosition() unknown direction - cant update position when state is REST_OFF");
            break;
        }

        // clip to 0-100 (because cant actually happen due to limit switches)
        if (positionNow < 0)
            positionNow = 0;
        else if (positionNow > 100)
            positionNow = 100;


        ESP_LOGD(TAG, "[%s] state='%s' - update pos from %.2f%% to %.2f%% (time ran %dms)", name, restStateStr[state], positionOld, positionNow, timeRan);

}



//============================
//========= setState =========
//============================
void cControlledRest::setState(restState_t targetState)
{
    //check if actually changed
    if (targetState == state){
        ESP_LOGV(TAG, "[%s] state already at '%s', nothing to do", name, restStateStr[state]);
        return;
    }

    // when switching direction without stop: update position first
    if (state != REST_OFF)
        updatePosition();

    // activate handle task when turning on (previous state is off)
    if (state == REST_OFF)
        xTaskNotifyGive(taskHandle); //activate handle task that stops the rest-motor again

    //apply new state
    ESP_LOGI(TAG, "[%s] switching from state '%s' to '%s'", name, restStateStr[state], restStateStr[targetState]);
    switch (targetState)
    {
    case REST_UP:
        gpio_set_level(gpio_down, 0);
        gpio_set_level(gpio_up, 1);
        timestamp_lastPosUpdate = esp_log_timestamp();
        break;
    case REST_DOWN:
        gpio_set_level(gpio_down, 1);
        gpio_set_level(gpio_up, 0);
        timestamp_lastPosUpdate = esp_log_timestamp();
        break;
    case REST_OFF:
        gpio_set_level(gpio_down, 0);
        gpio_set_level(gpio_up, 0);
        updatePosition();
        positionTarget = positionNow; //disable resuming - no unexpected pos when incrementing
        break;
    }
    state = targetState;
}



//==========================
//==== setTargetPercent ====
//==========================
void cControlledRest::setTargetPercent(float targetPercent){
    float positionTargetPrev = positionTarget;
    positionTarget = targetPercent;

    // limit to 0-100
    if (positionTarget > 100)
        positionTarget = 100;
    else if (positionTarget < 0)
        positionTarget = 0;
        
        ESP_LOGI(TAG, "[%s] changed Target position from %.2f%% to %.2f%%", name, positionTargetPrev, positionTarget);

    // start rest in required direction
    // TODO always run this check in handle()?
    // note: when already at 0/100 start anyways (runs for certain threshold in case tracked position out of sync)
    if (positionTarget > positionNow || positionTarget >= 100)
        setState(REST_UP);
    else if (positionTarget < positionNow || positionTarget <= 0)
        setState(REST_DOWN);
    else // already at exact position
        setState(REST_OFF);
}



//======================
//======= handle =======
//======================
// handle automatic stop when target position is reached, should be run repeatedly in a task
#define TRAVEL_TIME_LIMIT_ADDITION_MS 2000 // traveling longer into limit compensates inaccuracies in time based position tracking
void cControlledRest::handle(){

    // nothing to do when not running atm
    // TODO: turn on automatically when position != target?
    if (state == REST_OFF)
        return;

    // calculate time already running and needed time to reach target
    uint32_t timeRan = esp_log_timestamp() - timestamp_lastPosUpdate;
    uint32_t timeTarget = travelDuration * fabs(positionTarget - positionNow) / 100;

    // intentionally travel longer into limit - compensates inaccuracies in time based position tracking
    if (positionTarget == 0 || positionTarget == 100)
        timeTarget += TRAVEL_TIME_LIMIT_ADDITION_MS;

    // target reached
    if (timeRan >= timeTarget){
        ESP_LOGW(TAG, "[%s] reached target run-time (%dms/%dms) for position %.2f%% -> stopping", name, timeRan, timeTarget, positionTarget);
        setState(REST_OFF);
    }
}



//============================
//===== chairAdjust_task =====
//============================
#define CHAIR_ADJUST_HANDLE_TASK_DELAY 100
void chairAdjust_task( void * pvParameter )
{
    cControlledRest * rest = (cControlledRest *)pvParameter;
	ESP_LOGW(TAG, "Starting task for controlling %s...", rest->getName());
    rest->taskHandle = xTaskGetCurrentTaskHandle();

	while (1)
	{
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // wait for wakeup by setState() (rest-motor turned on)
        ESP_LOGW(TAG, "task %s: received notification -> activating task!", rest->getName());
        while (rest->getState() != REST_OFF){
            rest->handle();
            vTaskDelay(CHAIR_ADJUST_HANDLE_TASK_DELAY / portTICK_PERIOD_MS);
        }
        ESP_LOGW(TAG, "task %s: rest turned off -> sleeping task", rest->getName());
    }
}



//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
//TODO:
// - add separate task that controls chair adjustment
//    - timeout
//    - track position
//    - auto-adjust: move to position while driving
//    - control via app
// - add delay betweem direction change
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest){
	//--- variables ---
    float stickThreshold = 0.3; //min coordinate for motor to start

    //--- control rest motors ---
    //leg rest (x-axis)
    if (data.x > stickThreshold) legRest->setTargetPercent(100);
    else if (data.x < -stickThreshold) legRest->setTargetPercent(0);
    else legRest->setState(REST_OFF);

    //back rest (y-axis)
    if (data.y > stickThreshold) backRest->setTargetPercent(100);
    else if (data.y < -stickThreshold) backRest->setTargetPercent(0);
    else backRest->setState(REST_OFF);
}