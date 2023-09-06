#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"

#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "fan.hpp"


//in IDLE mode: set loglevel for evaluatedJoystick to DEBUG 
//and repeatedly read joystick e.g. for manually calibrating / testing joystick
//#define JOYSTICK_LOG_IN_IDLE


//TODO outsource global variables to e.g. global.cpp and only config options here?

//create global controlledMotor instances for both motors
extern controlledMotor motorLeft;
extern controlledMotor motorRight;

//create global buzzer object
extern buzzer_t buzzer;

//configuration for fans / cooling
extern fan_config_t configCooling;

