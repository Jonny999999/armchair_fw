#pragma once

#include "motordrivers.hpp"
#include "motorctl.hpp"
#include "joystick.hpp"

#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "control.hpp"


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

