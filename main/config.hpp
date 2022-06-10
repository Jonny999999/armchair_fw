#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "joystick.hpp"

#include "gpio_evaluateSwitch.hpp"


//create global controlledMotor instances for both motors
extern controlledMotor motorLeft;
extern controlledMotor motorRight;

//create global joystic instance
extern evaluatedJoystick joystick;

//create global evaluated switch instance for button next to joystick
extern gpio_evaluatedSwitch buttonJoystick;
