#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "joystick.hpp"

#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "control.hpp"
#include "fan.hpp"
#include "http.hpp"
#include "auto.hpp"


//in IDLE mode: set loglevel for evaluatedJoystick to DEBUG 
//and repeatedly read joystick e.g. for manually calibrating / testing joystick
#define JOYSTICK_LOG_IN_IDLE


//create global controlledMotor instances for both motors
extern controlledMotor motorLeft;
extern controlledMotor motorRight;

//create global joystic instance
extern evaluatedJoystick joystick;

//create global evaluated switch instance for button next to joystick
extern gpio_evaluatedSwitch buttonJoystick;

//create global buzzer object
extern buzzer_t buzzer;

//create global control object
extern controlledArmchair control;

//create global automatedArmchair object (for auto-mode)
extern automatedArmchair armchair;

//create global httpJoystick object
extern httpJoystick httpJoystickMain;

//configuration for fans
extern fan_config_t configFanLeft;
extern fan_config_t configFanRight;

