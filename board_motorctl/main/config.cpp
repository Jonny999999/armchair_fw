
#include "config.hpp"

//===================================
//======= motor configuration =======
//===================================
//--- configure left motor (hardware) ---
single100a_config_t configDriverLeft = {
    .gpio_pwm = GPIO_NUM_26,
    .gpio_a = GPIO_NUM_4,
    .gpio_b = GPIO_NUM_16,
	.gpio_brakeRelay = GPIO_NUM_5, //power mosfet 2
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
	.gpio_brakeRelay = GPIO_NUM_18, //power mosfet 1
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
	.currentLimitEnabled = false,
	.currentSensor_adc =  ADC1_CHANNEL_0, //GPIO36
	.currentSensor_ratedCurrent = 50,
    .currentMax = 30,
	.deadTimeMs = 900 //minimum time motor is off between direction change
};

//--- configure right motor (contol) ---
motorctl_config_t configMotorControlRight = {
    .msFadeAccel = 1900, //acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, //deceleration of the motor (ms it takes from 100% to 0%)
	.currentLimitEnabled = false,
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
//===== create global objects =====
//=================================
//TODO outsource global variables to e.g. global.cpp and only config options here?
single100a motorDriverLeft(configDriverLeft);
single100a motorDriverRight(configDriverRight);

//--- controlledMotor ---
//functions for updating the duty via certain/current driver that can then be passed to controlledMotor
//-> makes it possible to easily use different motor drivers
//note: ignoring warning "capture of variable with non-automatic storage duration", since sabertoothDriver object does not get destroyed anywhere - no lifetime issue
motorSetCommandFunc_t setLeftFunc = [&motorDriverLeft](motorCommand_t cmd) {
    motorDriverLeft.set(cmd);
};
motorSetCommandFunc_t setRightFunc = [&motorDriverRight](motorCommand_t cmd) {
    motorDriverRight.set(cmd);
};
//create controlled motor instances (motorctl.hpp)
controlledMotor motorLeft(setLeftFunc, configMotorControlLeft);
controlledMotor motorRight(setRightFunc, configMotorControlRight);

//create buzzer object on pin 12 with gap between queued events of 100ms 
buzzer_t buzzer(GPIO_NUM_12, 100);
