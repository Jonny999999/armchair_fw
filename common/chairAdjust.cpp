extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
}

#include "chairAdjust.hpp"




//--- config ---
//relays that control the motor for electric chair adjustment
//TODO: add this to config?
//relays connected to 4x stepper mosfets:
#define GPIO_LEGREST_UP     GPIO_NUM_4
#define GPIO_LEGREST_DOWN   GPIO_NUM_16
#define GPIO_BACKREST_UP    GPIO_NUM_2
#define GPIO_BACKREST_DOWN  GPIO_NUM_15

//--- variables ---
//tag for logging
static const char * TAG = "chair-adjustment";
//current motor states
static restState_t stateLegRest = REST_OFF;
static restState_t stateBackRest = REST_OFF;


//TODO Add timestamps or even a task to keep track of current position (estimate)




//=============================
//======= set direction =======
//=============================
//functions for each rest that set the motors to desired direction / state
//TODO evaluate if separate functions needed, can be merged with run..Rest(state) function?
//--- leg-rest ---
void setLegrestUp(){
        gpio_set_level(GPIO_LEGREST_DOWN, 0);
        gpio_set_level(GPIO_LEGREST_UP, 1);
        stateLegRest = REST_UP;
        ESP_LOGD(TAG, "switched relays to move leg-rest UP");
}
void setLegrestDown(){
        gpio_set_level(GPIO_LEGREST_DOWN, 1);
        gpio_set_level(GPIO_LEGREST_UP, 0);
        stateLegRest = REST_DOWN;
        ESP_LOGD(TAG, "switched relays to move leg-rest DOWN");
}
void setLegrestOff(){
        gpio_set_level(GPIO_LEGREST_DOWN, 0);
        gpio_set_level(GPIO_LEGREST_UP, 0);
        stateLegRest = REST_OFF;
        ESP_LOGD(TAG, "switched relays for leg-rest OFF");
}

//--- back-rest ---
void setBackrestUp(){
        gpio_set_level(GPIO_BACKREST_DOWN, 0);
        gpio_set_level(GPIO_BACKREST_UP, 1);
        stateBackRest = REST_UP;
        ESP_LOGD(TAG, "switched relays to move back-rest UP");
}
void setBackrestDown(){
        gpio_set_level(GPIO_BACKREST_DOWN, 1);
        gpio_set_level(GPIO_BACKREST_UP, 0);
        stateBackRest = REST_DOWN;
        ESP_LOGD(TAG, "switched relays to move back-rest DOWN");
}
void setBackrestOff(){
        gpio_set_level(GPIO_BACKREST_DOWN, 0);
        gpio_set_level(GPIO_BACKREST_UP, 0);
        stateBackRest = REST_OFF;
        ESP_LOGD(TAG, "switched relays for back-rest OFF");
}




//==============================
//========= runLegrest =========
//==============================
//abstract functions that can be used to set the state of leg rest
// 0 = OFF;  <0 = DOWN;  >0 = UP
void runLegrest(float targetDirection){
    if (targetDirection > 0) {
        setLegrestUp();
        ESP_LOGD(TAG, "Leg-rest: coordinate = %.1f => run UP", targetDirection);
    } else if (targetDirection < 0) {
        setLegrestDown();
        ESP_LOGD(TAG, "Leg-rest: coordinate = %.1f => run DOWN", targetDirection);
    } else {
        setLegrestOff();
        ESP_LOGD(TAG, "Leg-rest: coordinate = %.1f => OFF", targetDirection);
    }
}
//set to certain state
void runLegrest(restState_t targetState){
    switch (targetState){
        case REST_UP:
            setLegrestUp();
            break;
        case REST_DOWN:
            setLegrestDown();
            break;
        case REST_OFF:
            setLegrestOff();
            break;
    }
}


//==============================
//========= runBackrest =========
//==============================
//abstract functions that can be used to set the state of back rest
// 0 = OFF;  <0 = DOWN;  >0 = UP
void runBackrest(float targetDirection){
    if (targetDirection > 0) {
        setBackrestUp();
        ESP_LOGD(TAG, "Back-rest: coordinate = %.1f => run UP", targetDirection);
    } else if (targetDirection < 0) {
        setBackrestDown();
        ESP_LOGD(TAG, "Back-rest: coordinate = %.1f => run DOWN", targetDirection);
    } else {
        setBackrestOff();
        ESP_LOGD(TAG, "back-rest: coordinate = %.1f => off", targetDirection);
    }
}
//set to certain state
void runBackrest(restState_t targetState){
    switch (targetState){
        case REST_UP:
            setBackrestUp();
            break;
        case REST_DOWN:
            setBackrestDown();
            break;
        case REST_OFF:
            setBackrestOff();
            break;
    }
}
