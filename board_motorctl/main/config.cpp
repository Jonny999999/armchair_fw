#include "config.hpp"

//===================================
//======= motor configuration =======
//===================================
//--- configure left motor (hardware) ---
single100a_config_t configDriverLeft = {
    .gpio_pwm = GPIO_NUM_26,
    .gpio_a = GPIO_NUM_6,
    .gpio_b = GPIO_NUM_16,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
	.aEnabledPinState = false, //-> pins inverted (mosfets)
	.bEnabledPinState = false,
    .resolution = LEDC_TIMER_11_BIT,
    .pwmFreq = 10000
};

//--- configure right motor (hardware) ---
single100a_config_t configDriverRight = {
    .gpio_pwm = GPIO_NUM_27,
    .gpio_a = GPIO_NUM_2,
    .gpio_b = GPIO_NUM_15,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
	.aEnabledPinState = false, //-> pins inverted (mosfets)
	.bEnabledPinState = false,
    .resolution = LEDC_TIMER_11_BIT,
    .pwmFreq = 10000
};


//TODO add motor name string -> then use as log tag?
//--- configure left motor (contol) ---
motorctl_config_t configMotorControlLeft = {
    .msFadeAccel = 1900, //acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, //deceleration of the motor (ms it takes from 100% to 0%)
	.currentLimitEnabled = true,
	.currentSensor_adc =  ADC1_CHANNEL_0, //GPIO36
	.currentSensor_ratedCurrent = 50,
    .currentMax = 30,
	.deadTimeMs = 900 //minimum time motor is off between direction change
};

//--- configure right motor (contol) ---
motorctl_config_t configMotorControlRight = {
    .msFadeAccel = 1900, //acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, //deceleration of the motor (ms it takes from 100% to 0%)
	.currentLimitEnabled = true,
	.currentSensor_adc =  ADC1_CHANNEL_3, //GPIO39
	.currentSensor_ratedCurrent = 50,
    .currentMax = 30,
	.deadTimeMs = 900 //minimum time motor is off between direction change
};



//============================
//=== configure fan contol ===
//============================
fan_config_t configCooling = {
    .gpio_fan = GPIO_NUM_13,
    .dutyThreshold = 40,
	.minOnMs = 1500,
	.minOffMs = 3000,
	.turnOffDelayMs = 5000,
};




//=================================
//===== create global object s =====
//=================================
//TODO outsource global variables to e.g. global.cpp and only config options here?

//create controlled motor instances (motorctl.hpp)
controlledMotor motorLeft(configDriverLeft, configMotorControlLeft);
controlledMotor motorRight(configDriverRight, configMotorControlRight);

//create buzzer object on pin 12 with gap between queued events of 100ms 
buzzer_t buzzer(GPIO_NUM_12, 100);
