#pragma once

#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "control.hpp"
#include "motorctl.hpp"
#include "auto.hpp"
#include "joystick.hpp"
#include "chairAdjust.hpp"



//===================================
//====== buttonCommands class =======
//===================================
//class which runs commands depending on the count a button was pressed
class buttonCommands {
    public:
        //--- constructor ---
        buttonCommands(
            controlledArmchair *control_f,
            evaluatedJoystick *joystick_f,
            QueueHandle_t encoderQueue_f,
            controlledMotor * motorLeft_f,
            controlledMotor *motorRight_f,
            cControlledRest *legRest_f,
            cControlledRest *backRest_f,
            buzzer_t *buzzer_f);

        //--- functions ---
        //the following function has to be started once in a separate task. 
        //repeatedly evaluates and processes button events then takes the corresponding action
        void startHandleLoop(); 

    private:
        //--- functions ---
        void action(uint8_t count, bool lastPressLong);

        //--- objects ---
        controlledArmchair * control;
        evaluatedJoystick* joystick;
        controlledMotor * motorLeft;
        controlledMotor * motorRight;
        buzzer_t* buzzer;
        QueueHandle_t encoderQueue;
        cControlledRest *legRest;
        cControlledRest *backRest;

        //--- variables ---
        uint8_t count = 0;
        uint32_t timestamp_lastAction = 0;
        enum class inputState_t {IDLE, WAIT_FOR_INPUT};
        inputState_t state = inputState_t::IDLE;

};



//======================================
//============ button task =============
//======================================
// struct with variables passed to task from main
typedef struct task_button_parameters_t
{
    controlledArmchair *control;
    evaluatedJoystick *joystick;
    QueueHandle_t encoderQueue;
    controlledMotor *motorLeft;
    controlledMotor *motorRight;
    cControlledRest *legRest;
    cControlledRest *backRest;
    buzzer_t *buzzer;
} task_button_parameters_t;

//task that handles the button interface/commands
void task_button( void * task_button_parameters );