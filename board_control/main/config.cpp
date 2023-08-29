#include "config.hpp"

//==============================
//======= control config =======
//==============================
control_config_t configControl = {
    .defaultMode = controlMode_t::JOYSTICK, //default mode after startup and toggling IDLE
    //--- timeout ---    
    .timeoutMs = 5*60*1000,      //time of inactivity after which the mode gets switched to IDLE
    .timeoutTolerancePer = 5,    //percentage the duty can vary between timeout checks considered still inactive
    //--- http mode ---

};



//===============================
//===== httpJoystick config =====
//===============================
httpJoystick_config_t configHttpJoystickMain{
    .toleranceZeroX_Per = 1,  //percentage around joystick axis the coordinate snaps to 0
    .toleranceZeroY_Per = 6,
    .toleranceEndPer = 2,   //percentage before joystick end the coordinate snaps to 1/-1
    .timeoutMs = 2500       //time no new data was received before the motors get turned off
};



//======================================
//======= joystick configuration =======
//======================================
joystick_config_t configJoystick = {
    .adc_x = ADC1_CHANNEL_3, //GPIO39
    .adc_y = ADC1_CHANNEL_0, //GPIO36
    //percentage of joystick range the coordinate of the axis snaps to 0 (0-100)
    .tolerance_zeroX_per = 7, //6
    .tolerance_zeroY_per = 10, //7
    //percentage of joystick range the coordinate snaps to -1 or 1 before configured "_max" or "_min" threshold (mechanical end) is reached (0-100)
    .tolerance_end_per = 4, 
    //threshold the radius jumps to 1 before the stick is at max radius (range 0-1)
    .tolerance_radius = 0.09,

    //min and max adc values of each axis, !!!AFTER INVERSION!!! is applied:
    .x_min = 1392, //=> x=-1
    .x_max = 2650, //=> x=1
    .y_min = 1390, //=> y=-1
    .y_max = 2640, //=> y=1
    //invert adc measurement
    .x_inverted = true,
    .y_inverted = true
};




//=================================
//===== create global objects =====
//=================================
//TODO outsource global variables to e.g. global.cpp and only config options here?

//create global joystic instance (joystick.hpp)
evaluatedJoystick joystick(configJoystick);

//create global evaluated switch instance for button next to joystick
gpio_evaluatedSwitch buttonJoystick(GPIO_NUM_25, true, false); //pullup true, not inverted (switch to GND use pullup of controller)
                                                               
//create buzzer object on pin 12 with gap between queued events of 100ms 
buzzer_t buzzer(GPIO_NUM_12, 100);

//create global httpJoystick object (http.hpp)
httpJoystick httpJoystickMain(configHttpJoystickMain);

//create global control object (control.hpp)
//controlledArmchair control(configControl, &buzzer, &motorLeft, &motorRight, &joystick, &httpJoystickMain);

//create global automatedArmchair object (for auto-mode) (auto.hpp)
automatedArmchair armchair;


