#include "config.hpp"

//-----------------------------------
//------- motor configuration -------
//-----------------------------------
//--- configure left motor ---
single100a_config_t configDriverLeft = {
    .gpio_pwm = GPIO_NUM_26,
    .gpio_a = GPIO_NUM_16,
    .gpio_b = GPIO_NUM_4,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .abInverted = true,
    .resolution = LEDC_TIMER_11_BIT,
    .pwmFreq = 10000
};

//--- configure right motor ---
single100a_config_t configDriverRight = {
    .gpio_pwm = GPIO_NUM_27,
    .gpio_a = GPIO_NUM_18,
    .gpio_b = GPIO_NUM_14,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .abInverted = false,
    .resolution = LEDC_TIMER_11_BIT,
    .pwmFreq = 10000
};

//--- configure motor contol ---
motorctl_config_t configMotorControl = {
    .msFade = 900,
    .currentMax = 10
};

//create controlled motor instances
controlledMotor motorLeft(configDriverLeft, configMotorControl);
controlledMotor motorRight(configDriverRight, configMotorControl);



//------------------------------
//------- control config -------
//------------------------------
control_config_t configControl = {
    .defaultMode = controlMode_t::JOYSTICK, //default mode after startup and toggling IDLE
    //--- timeout ---    
    .timeoutMs = 5*60*1000,      //time of inactivity after which the mode gets switched to IDLE
    .timeoutTolerancePer = 5,    //percentage the duty can vary between timeout checks considered still inactive
    //--- http mode ---
    .http_toleranceZeroX_Per = 3,  //percentage around joystick axis the coordinate snaps to 0
    .http_toleranceZeroY_Per = 10,
    .http_toleranceEndPer = 2,   //percentage before joystick end the coordinate snaps to 1/-1
    .http_timeoutMs = 3000       //time no new data was received before the motors get turned off

};



//--------------------------------------
//------- joystick configuration -------
//--------------------------------------
joystick_config_t configJoystick = {
    .adc_x = ADC1_CHANNEL_3, //GPIO39
    .adc_y = ADC1_CHANNEL_0, //GPIO36
    //range around center-threshold of each axis the coordinates stays at 0 (percentage of available range 0-100)
    .tolerance_zero = 7,
    //threshold the coordinate snaps to -1 or 1 before configured "_max" or "_min" threshold (mechanical end) is reached (percentage of available range 0-100)
    .tolerance_end = 5, 
    //threshold the radius jumps to 1 before the stick is at max radius (range 0-1)
    .tolerance_radius = 0.05,

    //min and max adc values of each axis
    .x_min = 975,
    .x_max = 2520,
    .y_min = 1005,
    .y_max = 2550,
    //invert adc measurement
    .x_inverted = true,
    .y_inverted = true
};



//----------------------------
//--- configure fan contol ---
//----------------------------
fan_config_t configFanLeft = {
    .gpio_fan = GPIO_NUM_2,
    .msRun = 5000,
    .dutyThreshold = 35
};
fan_config_t configFanRight = {
    .gpio_fan = GPIO_NUM_15,
    .msRun = 5000,
    .dutyThreshold = 35
};



//=================================
//===== create global objects =====
//=================================
//create global joystic instance
evaluatedJoystick joystick(configJoystick);

//create global evaluated switch instance for button next to joystick
gpio_evaluatedSwitch buttonJoystick(GPIO_NUM_33, true, false); //pullup true, not inverted (switch to GND use pullup of controller)
                                                               
//create buzzer object on pin 12 with gap between queued events of 100ms 
buzzer_t buzzer(GPIO_NUM_12, 100);

//create global control object
controlledArmchair control(configControl, &buzzer, &motorLeft, &motorRight);



