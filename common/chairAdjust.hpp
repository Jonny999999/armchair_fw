#pragma once
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    void setState(restState_t targetState);
    restState_t getState() const {return state;};
    float getPercent(); //TODO update position first
    void setTargetPercent(float targetPercent);
    float getTargetPercent() const {return positionTarget;};
    void handle();
    const char * getName() const {return name;};
    TaskHandle_t taskHandle = NULL; //task that repeatedly runs the handle() method

private:
    void init();
    void updatePosition();

    char name[32];
    gpio_num_t gpio_up;
    gpio_num_t gpio_down;
    restState_t state;
    const uint32_t travelDuration = 12000;
    uint32_t timestamp_lastPosUpdate = 0;
    float positionTarget = 0;
    float positionNow = 0;

};



//===========================
//==== chairAdjust_task =====
//===========================
// repeatedly runs handle method of specified ControlledRest object to turn of the rest, when activated by setState()
void chairAdjust_task( void * cControlledRest );



//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest);