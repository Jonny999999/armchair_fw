extern "C"
{
#include "esp_log.h"
#include <string.h>
}

#include "chairAdjust.hpp"


#define MUTEX_TIMEOUT (5000 / portTICK_PERIOD_MS)
#define MIN_OFF_TIME_DIR_CHANGE 400 // time off between direction change
//TODO: #define MIN_OFF_TIME dont allow instant resume in same diretion (apparently relays short and fuse blows)
#define MIN_ON_TIME 200

//--- gloabl variables ---
// strings for logging the rest state
const char* restStateStr[] = {"REST_OFF", "REST_DOWN", "REST_UP"};

//--- local variables ---
//tag for logging
static const char * TAG = "chair-adjustment";



//=============================
//======== constructor ========
//=============================
cControlledRest::cControlledRest(gpio_num_t gpio_up_f, gpio_num_t gpio_down_f, uint32_t travelDurationMs, const char * name_f, float defaultPosition): gpio_up(gpio_up_f), gpio_down(gpio_down_f), travelDuration(travelDurationMs){
    strcpy(name, name_f);
    positionNow = defaultPosition;
    positionTarget = positionNow;
    // recursive mutex necessary, because handle() method calls setState() which both have the same mutex
    mutex = xSemaphoreCreateRecursiveMutex();
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
    ESP_LOGV(TAG, "setState START");
    // lock the mutex before accessing shared variables
    if (xSemaphoreTakeRecursive(mutex, MUTEX_TIMEOUT) == pdTRUE)
    {
        // TODO: drop this section?
        // check if actually changed
        if (targetState == state)
        {
            // update anyways when target is 0 or 100, to trigger movement threshold in case of position tracking is out of sync
            if (state != REST_OFF && (positionTarget == 0 || positionTarget == 100))
                ESP_LOGD(TAG, "[%s] state already at '%s', but updating anyway to trigger move to limit addition", name, restStateStr[state]);
            else
            {
                ESP_LOGV(TAG, "[%s] state already at '%s', nothing to do", name, restStateStr[state]);
                // Release the mutex
                xSemaphoreGiveRecursive(mutex);
                return;
            }
        }

        // previous state
        switch (state)
        {
        case REST_UP:
            updatePosition();
            timestampLastUp = esp_log_timestamp();
            break;
        case REST_DOWN:
            updatePosition();
            timestampLastDown = esp_log_timestamp();
            break;
        case REST_OFF:
            break;
        }


        // ensure certain time off between different directions
        // TODO move this delay to adjust-task to not block other tasks with setPercentage()
        uint32_t msSinceOtherDir = 0;
        if (targetState == REST_UP)
            msSinceOtherDir = esp_log_timestamp() - timestampLastDown;
        else if (targetState == REST_DOWN)
            msSinceOtherDir = esp_log_timestamp() - timestampLastUp;
        if ((msSinceOtherDir < MIN_OFF_TIME_DIR_CHANGE) && targetState != REST_OFF)
        {
            // turn off and wait for remaining time
            ESP_LOGW(TAG, "[%s] too fast direction change detected, waiting %d ms in REST_OFF before switching to '%s'", name, MIN_OFF_TIME_DIR_CHANGE - msSinceOtherDir, restStateStr[targetState]);
            ESP_LOGV(TAG, "TURN-OFF-until-dir-change");
            // note: can not recursively use setState(REST_OFF) here, results in blown relay/fuse because turns briefly off (on/off/on) by task within 50ms due to positionTarget = positionNow
            gpio_set_level(gpio_down, 0);
            gpio_set_level(gpio_up, 0);
            if (state != REST_OFF) // no need to update if was turned off already
                updatePosition();
            vTaskDelay((MIN_OFF_TIME_DIR_CHANGE - msSinceOtherDir) / portTICK_PERIOD_MS);
        }

        // apply new state
        ESP_LOGI(TAG, "[%s] switching from state '%s' to '%s'", name, restStateStr[state], restStateStr[targetState]);
        switch (targetState)
        {
        case REST_UP:
            gpio_set_level(gpio_down, 0);
            gpio_set_level(gpio_up, 1);
            timestamp_lastPosUpdate = esp_log_timestamp();
            ESP_LOGV(TAG, "TURN-UP");
            break;
        case REST_DOWN:
            gpio_set_level(gpio_down, 1);
            gpio_set_level(gpio_up, 0);
            ESP_LOGV(TAG, "TURN-DOWN");
            timestamp_lastPosUpdate = esp_log_timestamp();
            break;
        case REST_OFF:
            gpio_set_level(gpio_down, 0);
            gpio_set_level(gpio_up, 0);
            ESP_LOGV(TAG, "TURN-OFF");
            if (state != REST_OFF)
                updatePosition();
            positionTarget = positionNow; // disable resuming - no unexpected pos when incrementing
            break;
        }

        // activate handle task when turning on (previous state is off)
        if (state == REST_OFF && targetState != REST_OFF)
            xTaskNotifyGive(taskHandle); // activate handle task that stops the rest-motor again
        
        state = targetState;

        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout in setState() -> RESTART");
        esp_restart();
    }
    ESP_LOGV(TAG, "setState END");
}

//==========================
//==== setTargetPercent ====
//==========================
void cControlledRest::setTargetPercent(float targetPercent)
{
    ESP_LOGV(TAG, "setTargetPercent START");
    // lock the mutex before accessing shared variables
    if (xSemaphoreTakeRecursive(mutex, MUTEX_TIMEOUT) == pdTRUE)
    {
        float positionTargetPrev = positionTarget;
        positionTarget = targetPercent;

        // limit to 0-100
        if (positionTarget > 100)
            positionTarget = 100;
        else if (positionTarget < 0)
            positionTarget = 0;

        // ignore if unchanged
        // if (positionTarget == positionTargetPrev){
        //    ESP_LOGI(TAG, "[%s] Target position unchanged at %.2f%%", name, positionTarget);
        //    return; //FIXME: free mutex
        //}

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

        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout while waiting in setTargetPercent -> RESTART");
        esp_restart();
    }
    ESP_LOGV(TAG, "setTargetPercent STOP");
}

//======================
//======= handle =======
//======================
// handle automatic stop when target position is reached, should be run repeatedly in a task
#define TRAVEL_TIME_LIMIT_ADDITION_MS 2000 // traveling longer into limit compensates inaccuracies in time based position tracking
void cControlledRest::handle()
{
    ESP_LOGV(TAG, "handle START");
    // lock the mutex before accessing shared variables
    if (xSemaphoreTakeRecursive(mutex, MUTEX_TIMEOUT) == pdTRUE)
    {
        // nothing to do when not running atm
        // TODO: turn on automatically when position != target?
        if (state == REST_OFF)
        {
            // Release the mutex
            xSemaphoreGiveRecursive(mutex);
            return;
        }

        // calculate time already running and needed time to reach target
        uint32_t timeRan = esp_log_timestamp() - timestamp_lastPosUpdate;
        uint32_t timeTarget = travelDuration * fabs(positionTarget - positionNow) / 100;

        // intentionally travel longer into limit - compensates inaccuracies in time based position tracking
        if (positionTarget == 0 || positionTarget == 100)
            timeTarget += TRAVEL_TIME_LIMIT_ADDITION_MS;

        // target reached
        if (timeRan >= timeTarget && timeRan >= MIN_ON_TIME)
        {
            ESP_LOGW(TAG, "[%s] handle: reached target run-time (%dms/%dms) for position %.2f%% -> stopping", name, timeRan, timeTarget, positionTarget);
            setState(REST_OFF);
        }

        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout while waiting in handle() -> RESTART");
        esp_restart();
    }
    ESP_LOGV(TAG, "handle STOP");

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
        ESP_LOGD(TAG, "task %s: received notification -> activating task!", rest->getName());
        while (rest->getState() != REST_OFF){
            rest->handle();
            vTaskDelay(CHAIR_ADJUST_HANDLE_TASK_DELAY / portTICK_PERIOD_MS);
        }
        ESP_LOGD(TAG, "task %s: rest turned off -> sleeping task", rest->getName());
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