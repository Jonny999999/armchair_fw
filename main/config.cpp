#include "config.hpp"


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

motorctl_config_t configControlLeft = {
    .msFade = 5000,
    .currentMax = 10
};

//create controlled motor
controlledMotor motorLeft(configDriverLeft, configControlLeft);
