extern "C"
{
#include "esp_log.h"
#include <string.h>
}

#include "chairAdjust.hpp"


#define MUTEX_TIMEOUT (10000 / portTICK_PERIOD_MS)

#define MIN_TIME_ON 1000
#define MIN_TIME_OFF 2000

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
void cControlledRest::updatePosition()
{
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
        // no change
        ESP_LOGW(TAG, "updatePosition() unknown direction - cant update position when state is REST_OFF");
        return;
    }

    // clip to 0-100 (because cant actually happen due to limit switches)
    if (positionNow < 0)
        positionNow = 0;
    else if (positionNow > 100)
        positionNow = 100;

    ESP_LOGD(TAG, "[%s] state='%s' - update pos from %.2f%% to %.2f%% (time ran %dms)", name, restStateStr[state], positionOld, positionNow, timeRan);
}

//==========================
//==== setTargetPercent ====
//==========================
void cControlledRest::setTargetPercent(float targetPercent)
{
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

        ESP_LOGI(TAG, "[%s] changed Target position from %.2f%% to %.2f%%", name, positionTargetPrev, positionTarget);

        // update actual position positionNow first when already running
        if (state != REST_OFF)
            updatePosition();

        // start rest in required direction
        // TODO always run this check in handle()?
        // note: when already at 0/100 start anyways (runs for certain threshold in case tracked position out of sync)
        if (positionTarget > positionNow || positionTarget >= 100)
            requestStateChange(REST_UP);
        else if (positionTarget < positionNow || positionTarget <= 0)
            requestStateChange(REST_DOWN);
        else // already at exact position
            requestStateChange(REST_OFF);

        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout while waiting in setTargetPercent -> RESTART");
        esp_restart();
    }
}



//============================
//==== requestStateChange ====
//============================
// queue state change that is executed when valid (respecting min thresholds)
void cControlledRest::requestStateChange(restState_t targetState)
{
    // lock the mutex before accessing shared variables
    if (xSemaphoreTakeRecursive(mutex, MUTEX_TIMEOUT) == pdTRUE)
    {
        ESP_LOGV(TAG, "[%s] requesting change to state '%s'", name, restStateStr[targetState]);
        nextState = targetState;

        // activate handle to process the request when on running already
        if (state == REST_OFF)
            xTaskNotifyGive(taskHandle); // activate handle task that stops the rest-motor again
        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout while waiting in requestStateChange -> RESTART");
        esp_restart();
    }
}



//===========================
//==== handleStateChange ====
//===========================
// ensure MIN_TIME_ON and MIN_TIME_OFF has passed between doing the requested state change
// repeatedly run by task if state is requested
void cControlledRest::handleStateChange()
{
    // lock the mutex before accessing shared variables
    if (xSemaphoreTakeRecursive(mutex, MUTEX_TIMEOUT) == pdTRUE)
    {
        uint32_t now = esp_log_timestamp();

        // already at target state
        if (state == nextState)
        {
            // exit, nothing todo
            xSemaphoreGiveRecursive(mutex);
            return;
        }

        // turn off requested
        else if (nextState == REST_OFF)
        {
            // exit if not on long enough
            if (now - timestamp_lastStateChange < MIN_TIME_ON)
            {
                positionTarget = positionNow; // disable resuming - no unexpected pos when incrementing
                ESP_LOGV(TAG, "SC: not on long enough, not turning off yet");
                xSemaphoreGiveRecursive(mutex);
                return;
            }
        }

        // turn on requested
        else if (state == REST_OFF && nextState != REST_OFF)
        {
            // exit if not off long enough
            if (now - timestamp_lastStateChange < MIN_TIME_OFF)
            {
                ESP_LOGV(TAG, "SC: not OFF long enough, not turning on yet");
                xSemaphoreGiveRecursive(mutex);
                return;
            }
        }

        // direction change requested
        else if (state != REST_OFF && nextState != REST_OFF)
        {
            // exit if not on long enough
            if (now - timestamp_lastStateChange < MIN_TIME_ON)
            {
                ESP_LOGV(TAG, "SC: dir change detected: not ON long enough, not turning off yet");
                xSemaphoreGiveRecursive(mutex);
                return;
            }
            // no immediate dir change, turn off first
            else
            {
                ESP_LOGD(TAG, "SC: dir change detected: turning off first");
                changeState(REST_OFF);
                xSemaphoreGiveRecursive(mutex);
                return;
            }
        }

        // not exited by now = no reason to prevent the state change -> update state!
        ESP_LOGV(TAG, "SC: change is allowed now -> applying new state '%s'", restStateStr[nextState]);
        changeState(nextState);

        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout while waiting in handleStateChange -> RESTART");
        esp_restart();
    }
}



//=========================
//====== changeState ======
//=========================
// change state (relays, timestamp) without any validation whether change is allowed
void cControlledRest::changeState(restState_t newState)
{
    // check if actually changed
    if (newState == state)
    {
        ESP_LOGV(TAG, "[%s] state already at '%s', nothing to do", name, restStateStr[state]);
        return;
    }

    // apply new state to relays
    ESP_LOGI(TAG, "[%s] switching Relays from state '%s' to '%s'", name, restStateStr[state], restStateStr[newState]);
    switch (newState)
    {
    case REST_UP:
        gpio_set_level(gpio_down, 0);
        gpio_set_level(gpio_up, 1);
        break;
    case REST_DOWN:
        gpio_set_level(gpio_down, 1);
        gpio_set_level(gpio_up, 0);
        break;
    case REST_OFF:
        gpio_set_level(gpio_down, 0);
        gpio_set_level(gpio_up, 0);
        break;
    }

    // apply new state to variables
    timestamp_lastStateChange = esp_log_timestamp();
    updatePosition();
    state = newState;
}



//===============================
//=== handle StopAtPosReached ===
//===============================
// handle automatic stop when target position is reached, should be run repeatedly in a task
#define TRAVEL_TIME_LIMIT_ADDITION_MS 2000 // traveling longer into limit compensates inaccuracies in time based position tracking
void cControlledRest::handleStopAtPosReached()
{
    // lock the mutex before accessing shared variables
    if (xSemaphoreTakeRecursive(mutex, MUTEX_TIMEOUT) == pdTRUE)
    {
        // nothing to do when not running atm
        if (state == REST_OFF)
        {
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
        if (timeRan >= timeTarget)
        {
            ESP_LOGW(TAG, "[%s] reached target! run-time (%dms/%dms) for position %.2f%% -> requesting stop", name, timeRan, timeTarget, positionTarget);
            requestStateChange(REST_OFF);
        }
        // Release the mutex
        xSemaphoreGiveRecursive(mutex);
    }
    else
    {
        ESP_LOGE(TAG, "mutex timeout while waiting in handleStopAtPosReached -> RESTART");
        esp_restart();
    }
}

//============================
//===== chairAdjust_task =====
//============================
#define CHAIR_ADJUST_HANDLE_TASK_DELAY 100
void chairAdjust_task(void *pvParameter)
{
    cControlledRest *rest = (cControlledRest *)pvParameter;
    ESP_LOGW(TAG, "Starting task for controlling %s...", rest->getName());
    // provide taskHandle to rest object for wakeup
    rest->taskHandle = xTaskGetCurrentTaskHandle();

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // wait for wakeup by changeState() (rest-motor turned on)
        ESP_LOGW(TAG, "task %s: received notification -> activating task!", rest->getName());
        // running while 1. motor running  or  2. not in target state yet
        while ((rest->getState() != REST_OFF) || (rest->getNextState() != rest->getState()))
        {
            rest->handleStateChange();
            rest->handleStopAtPosReached();
            vTaskDelay(CHAIR_ADJUST_HANDLE_TASK_DELAY / portTICK_PERIOD_MS);
        }
        ESP_LOGW(TAG, "task %s: motor-off and at target state -> sleeping task", rest->getName());
    }
}



//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
//TODO:
//    - control via app
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest){
	//--- variables ---
    float stickThreshold = 0.3; //min coordinate for motor to start

    //--- control rest motors ---
    //leg rest (x-axis)
    if (data.x > stickThreshold) legRest->setTargetPercent(100);
    else if (data.x < -stickThreshold) legRest->setTargetPercent(0);
    else legRest->requestStateChange(REST_OFF);

    //back rest (y-axis)
    if (data.y > stickThreshold) backRest->setTargetPercent(100);
    else if (data.y < -stickThreshold) backRest->setTargetPercent(0);
    else backRest->requestStateChange(REST_OFF);
}