#include "config.hpp"

//-----------------------------------
//------- motor configuration -------
//-----------------------------------
//configure motor driver
single100a_config_t configDriverLeft = {
    .gpio_pwm = GPIO_NUM_14,
    .gpio_a = GPIO_NUM_12,
    .gpio_b = GPIO_NUM_13,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .abInverted = true,
    .resolution = LEDC_TIMER_11_BIT,
    .pwmFreq = 10000
};

//configure motor contol
motorctl_config_t configControlLeft = {
    .msFade = 3000,
    .currentMax = 10
};

//create controlled motor
controlledMotor motorLeft(configDriverLeft, configControlLeft);




//--------------------------------------
//------- joystick configuration -------
//--------------------------------------
joystick_config_t configJoystick = {
    .adc_x = ADC1_CHANNEL_3, //GPIO39
    .adc_y = ADC1_CHANNEL_0, //GPIO36
    //range around center-threshold of each axis the coordinates stays at 0 (adc value 0-4095)
    .tolerance_zero = 100,
    //threshold the coordinate snaps to -1 or 1 before configured "_max" or "_min" threshold (mechanical end) is reached (adc value 0-4095)
    .tolerance_end = 80,
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

//create global joystic instance
evaluatedJoystick joystick(configJoystick);
