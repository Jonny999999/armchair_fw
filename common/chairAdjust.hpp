#pragma once
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
}

#include "joystick.hpp"


typedef enum {
    REST_OFF = 0,
    REST_DOWN,
    REST_UP
} restState_t;

extern const char* restStateStr[];



//=====================================
//======= cControlledRest class =======
//=====================================
//class that controls 2 relays powering a motor that moves a rest of the armchair up or down
//2 instances will be created one for back and one for leg rest
class cControlledRest {
public:
    cControlledRest(gpio_num_t gpio_up, gpio_num_t gpio_down, uint32_t travelDurationMs, const char *name, float defaultPosition = 0);

    // control the rest:
    void requestStateChange(restState_t targetState); //mutex
    restState_t getState() const {return state;};
    const char * getName() const {return name;};
    void setTargetPercent(float targetPercent); //mutex
    float getTargetPercent() const {return positionTarget;};
    float getPercent(); //current position in percent (updates tracked position first when currently moving)

    // required for task controlling the rest:
    void setTaskHandle(TaskHandle_t handle) {taskHandle = handle;};
    void handleStopAtPosReached(); //mutex
    void handleStateChange(); //mutex
    restState_t getNextState() const {return nextState;};


private:
    void init();
    void updatePosition();
    void changeState(restState_t newState);

    // task related:
    TaskHandle_t taskHandle = NULL; //task that repeatedly runs the handle() method, is assigned at task creation
    SemaphoreHandle_t mutex;

    // config:
    char name[32];
    const gpio_num_t gpio_up;
    const gpio_num_t gpio_down;
    const uint32_t travelDuration = 12000;

    // variables:
    restState_t state = REST_OFF;
    restState_t nextState = REST_OFF;
    uint32_t timestamp_lastStateChange = 0;
    uint32_t timestamp_lastPosUpdate = 0;
    // reference the automatic stop is calculated from - deliberately separate from
    // timestamp_lastPosUpdate, which also gets reset by readers calling getPercent()
    uint32_t timestamp_travelStart = 0;
    float positionAtTravelStart = 0;
    float positionTarget = 0;
    float positionNow = 0;

};



//===========================
//==== chairAdjust_task =====
//===========================
// repeatedly runs handle methods of specified ControlledRest object to turn of the rest, when activated by changeState() method
void chairAdjust_task( void * cControlledRest );



//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest);