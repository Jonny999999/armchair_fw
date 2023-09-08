#include "config.hpp"

//===================================
//======= motor configuration =======
//===================================
/* ==> currently using other driver
//--- configure left motor (hardware) ---
single100a_config_t configDriverLeft = {
    .gpio_pwm = GPIO_NUM_26,
    .gpio_a = GPIO_NUM_16,
    .gpio_b = GPIO_NUM_4,
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
    .gpio_b = GPIO_NUM_14,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
	.aEnabledPinState = false, //-> pin inverted (mosfet)
	.bEnabledPinState = true,  //-> not inverted (direct)
    .resolution = LEDC_TIMER_11_BIT,
    .pwmFreq = 10000
	};
	*/

//--- configure sabertooth driver --- (controls both motors in one instance)
sabertooth2x60_config_t sabertoothConfig = {
	.gpio_TX = GPIO_NUM_23,
	.uart_num = UART_NUM_2
};


//TODO add motor name string -> then use as log tag?
//--- configure left motor (contol) ---
motorctl_config_t configMotorControlLeft = {
    .msFadeAccel = 1900, //acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, //deceleration of the motor (ms it takes from 100% to 0%)
	.currentLimitEnabled = true,
	.currentSensor_adc =  ADC1_CHANNEL_6, //GPIO34
	.currentSensor_ratedCurrent = 50,
    .currentMax = 30,
	.deadTimeMs = 900 //minimum time motor is off between direction change
};

//--- configure right motor (contol) ---
motorctl_config_t configMotorControlRight = {
    .msFadeAccel = 1900, //acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, //deceleration of the motor (ms it takes from 100% to 0%)
	.currentLimitEnabled = true,
	.currentSensor_adc =  ADC1_CHANNEL_4, //GPIO32
	.currentSensor_ratedCurrent = 50,
    .currentMax = 30,
	.deadTimeMs = 900 //minimum time motor is off between direction change
};



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
//create sabertooth motor driver instance
sabertooth2x60a sabertoothDriver(sabertoothConfig);


//--- controlledMotor ---
//functions for updating the duty via certain/current driver that can then be passed to controlledMotor
//-> makes it possible to easily use different motor drivers
//note: ignoring warning "capture of variable 'sabertoothDriver' with non-automatic storage duration", since sabertoothDriver object does not get destroyed anywhere - no lifetime issue
motorSetCommandFunc_t setLeftFunc = [&sabertoothDriver](motorCommand_t cmd) {
    sabertoothDriver.setLeft(cmd);
};
motorSetCommandFunc_t setRightFunc = [&sabertoothDriver](motorCommand_t cmd) {
    sabertoothDriver.setRight(cmd);
};
//create controlled motor instances (motorctl.hpp)
controlledMotor motorLeft(setLeftFunc, configMotorControlLeft);
controlledMotor motorRight(setRightFunc, configMotorControlRight);


//create global joystic instance (joystick.hpp)
evaluatedJoystick joystick(configJoystick);

//create global evaluated switch instance for button next to joystick
gpio_evaluatedSwitch buttonJoystick(GPIO_NUM_25, true, false); //pullup true, not inverted (switch to GND use pullup of controller)
                                                               
//create buzzer object on pin 12 with gap between queued events of 100ms 
buzzer_t buzzer(GPIO_NUM_12, 100);

//create global httpJoystick object (http.hpp)
httpJoystick httpJoystickMain(configHttpJoystickMain);

//create global control object (control.hpp)
controlledArmchair control(configControl, &buzzer, &motorLeft, &motorRight, &joystick, &httpJoystickMain);

//create global automatedArmchair object (for auto-mode) (auto.hpp)
automatedArmchair armchair;


