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
    cControlledRest(gpio_num_t gpio_up, gpio_num_t gpio_down, uint32_t travelDurationMs, const char *name);
    void setState(restState_t targetState);
    float getPercent(); //TODO update position first
    void setTargetPercent(float targetPercent);
    float getTargetPercent() const {return positionTarget;};
    void handle();

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

// struct with variables passed to task from main()
typedef struct chairAdjust_task_parameters_t {
    cControlledRest * legRest;
    cControlledRest * backRest;
    //buzzer_t *buzzer;
} chairAdjust_task_parameters_t;



//===========================
//==== chairAdjust_task =====
//===========================
void chairAdjust_task( void * chairAdjust_task_parameters_t );



//====================================
//====== controlChairAdjustment ======
//====================================
//function that controls the two rests according to joystick data (applies threshold, defines direction)
void controlChairAdjustment(joystickData_t data, cControlledRest * legRest, cControlledRest * backRest);