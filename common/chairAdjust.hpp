#pragma once

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
    cControlledRest(gpio_num_t gpio_up, gpio_num_t gpio_down, const char *name);
    void setState(restState_t targetState);
    void stop();

private:
    void init();

    char name[32];
    gpio_num_t gpio_up;
    gpio_num_t gpio_down;
    restState_t state;
    const uint32_t travelDuration = 5000;
    uint32_t timestamp_lastChange;
    float currentPosition = 0;
};

//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest);