#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "joystick.hpp"

//create global controlledMotor instances for both motors
extern controlledMotor motorLeft;
extern controlledMotor motorRight;

//create global joystic instance
extern evaluatedJoystick joystick;
