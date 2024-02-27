// NOTE: this file is included in main.cpp only.
// outsourced all configuration related functions and structures to this file:

extern "C"
{
#include "esp_log.h"
}
#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "joystick.hpp"
#include "http.hpp"
#include "speedsensor.hpp"
#include "buzzer.hpp"
#include "control.hpp"
#include "fan.hpp"
#include "auto.hpp"
#include "chairAdjust.hpp"
#include "display.hpp"
#include "encoder.h"

//==================================
//======== define loglevels ========
//==================================
void setLoglevels(void)
{
    // set loglevel for all tags:
    esp_log_level_set("*", ESP_LOG_WARN);

    //--- set loglevel for individual tags ---
    esp_log_level_set("main", ESP_LOG_INFO);
    esp_log_level_set("buzzer", ESP_LOG_ERROR);
    // esp_log_level_set("motordriver", ESP_LOG_DEBUG);
    esp_log_level_set("motor-control", ESP_LOG_WARN);
    // esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
    // esp_log_level_set("joystickCommands", ESP_LOG_DEBUG);
    esp_log_level_set("button", ESP_LOG_INFO);
    esp_log_level_set("control", ESP_LOG_INFO);
    // esp_log_level_set("fan-control", ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_INFO);
    esp_log_level_set("http", ESP_LOG_INFO);
    // esp_log_level_set("automatedArmchair", ESP_LOG_DEBUG);
    esp_log_level_set("display", ESP_LOG_INFO);
    // esp_log_level_set("current-sensors", ESP_LOG_INFO);
    esp_log_level_set("speedSensor", ESP_LOG_WARN);
    esp_log_level_set("chair-adjustment", ESP_LOG_INFO);
    esp_log_level_set("menu", ESP_LOG_INFO);
    esp_log_level_set("encoder", ESP_LOG_INFO);
}

//==================================
//======== configuration ===========
//==================================

//-----------------------------------
//------- motor configuration -------
//-----------------------------------
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
    .gpio_TX = GPIO_NUM_27,
    .uart_num = UART_NUM_2};

// TODO add motor name string -> then use as log tag?
//--- configure left motor (contol) ---
motorctl_config_t configMotorControlLeft = {
    .name = "left",
    .msFadeAccel = 1500, // acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, // deceleration of the motor (ms it takes from 100% to 0%)
    .currentLimitEnabled = false,
    .currentSensor_adc = ADC1_CHANNEL_4, // GPIO32
    .currentSensor_ratedCurrent = 50,
    .currentMax = 30,
    .currentInverted = true,
    .currentSnapToZeroThreshold = 0.15,
    .deadTimeMs = 0 // minimum time motor is off between direction change
};

//--- configure right motor (contol) ---
motorctl_config_t configMotorControlRight = {
    .name = "right",
    .msFadeAccel = 1500, // acceleration of the motor (ms it takes from 0% to 100%)
    .msFadeDecel = 1000, // deceleration of the motor (ms it takes from 100% to 0%)
    .currentLimitEnabled = false,
    .currentSensor_adc = ADC1_CHANNEL_5, // GPIO33
    .currentSensor_ratedCurrent = 50,
    .currentMax = 30,
    .currentInverted = false,
    .currentSnapToZeroThreshold = 0.25,
    .deadTimeMs = 0 // minimum time motor is off between direction change
};

//------------------------------
//------- control config -------
//------------------------------
control_config_t configControl = {
    .defaultMode = controlMode_t::JOYSTICK, // default mode after startup and toggling IDLE
    //--- timeout ---
    .timeoutMs = 3 * 60 * 1000, // time of inactivity after which the mode gets switched to IDLE
    .timeoutTolerancePer = 5,   // percentage the duty can vary between timeout checks considered still inactive
    //--- http mode ---

};

//-------------------------------
//----- httpJoystick config -----
//-------------------------------
httpJoystick_config_t configHttpJoystickMain{
    .toleranceZeroX_Per = 1, // percentage around joystick axis the coordinate snaps to 0
    .toleranceZeroY_Per = 6,
    .toleranceEndPer = 2, // percentage before joystick end the coordinate snaps to 1/-1
    .timeoutMs = 2500     // time no new data was received before the motors get turned off
};

//--------------------------------------
//------- joystick configuration -------
//--------------------------------------
joystick_config_t configJoystick = {
    .adc_x = ADC1_CHANNEL_0, // GPIO36
    .adc_y = ADC1_CHANNEL_3, // GPIO39
    // percentage of joystick range the coordinate of the axis snaps to 0 (0-100)
    .tolerance_zeroX_per = 7,  // 6
    .tolerance_zeroY_per = 10, // 7
    // percentage of joystick range the coordinate snaps to -1 or 1 before configured "_max" or "_min" threshold (mechanical end) is reached (0-100)
    .tolerance_end_per = 4,
    // threshold the radius jumps to 1 before the stick is at max radius (range 0-1)
    .tolerance_radius = 0.09,

    // min and max adc values of each axis, !!!AFTER INVERSION!!! is applied:
    .x_min = 1710, //=> x=-1
    .x_max = 2980, //=> x=1
    .y_min = 1700, //=> y=-1
    .y_max = 2940, //=> y=1
    // invert adc measurement
    .x_inverted = false,
    .y_inverted = true};

//----------------------------
//--- configure fan contol ---
//----------------------------
fan_config_t configFans = {
    .gpio_fan = GPIO_NUM_13,
    .dutyThreshold = 40,
    .minOnMs = 1500,
    .minOffMs = 3000,
    .turnOffDelayMs = 5000,
};



//--------------------------------------------
//-------- speed sensor configuration --------
//--------------------------------------------
speedSensor_config_t speedLeft_config{
    .gpioPin = GPIO_NUM_5,
    .degreePerGroup = 360 / 16,
	.minPulseDurationUs = 3000, //smallest possible pulse duration (< time from start small-pulse to start long-pulse at full speed). Set to 0 to disable this noise detection
    //measured wihth scope while tires in the air:
    // 5-groups: 12ms
    // 16-groups: 3.7ms
    .tireCircumferenceMeter = 0.81,
    .directionInverted = true,
    .logName = "speedLeft"
};

speedSensor_config_t speedRight_config{
    .gpioPin = GPIO_NUM_14,
    .degreePerGroup = 360 / 12,
	.minPulseDurationUs = 4000, //smallest possible pulse duration (< time from start small-pulse to start long-pulse at full speed). Set to 0 to disable this noise detection
    .tireCircumferenceMeter = 0.81,
    .directionInverted = false,
    .logName = "speedRight"
};



//-------------------------
//-------- display --------
//-------------------------
display_config_t display_config {
    .gpio_scl = GPIO_NUM_22,
    .gpio_sda = GPIO_NUM_23,
    .gpio_reset = -1, //negative number disables reset feature
    .width = 128,
    .height = 64,
    .offsetX = 2,
    .flip = false,
    .contrast = 0xff, //max: 255
};



//-------------------------
//-------- encoder --------
//-------------------------
//configure rotary encoder (next to joystick)
rotary_encoder_t encoder_config = {
	.pin_a = GPIO_NUM_25,
	.pin_b = GPIO_NUM_26,
	.pin_btn = GPIO_NUM_21,
	.code = 1,
	.store = 0, //encoder count
	.index = 0,
	.btn_pressed_time_us = 20000,
	.btn_state = RE_BTN_RELEASED //default state
};


//-----------------------------------
//--- joystick command generation ---
//-----------------------------------
//configure parameters for motor command generation from joystick data
joystickGenerateCommands_config_t joystickGenerateCommands_config{
    .maxDuty = 100,
    .dutyOffset = 5, // duty at which motors start immediately
    .altStickMapping = false,
};