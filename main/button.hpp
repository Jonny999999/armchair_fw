#pragma once

#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "control.hpp"
#include "motorctl.hpp"



//===================================
//====== buttonCommands class =======
//===================================
//class which runs commands depending on the count a button was pressed
class buttonCommands {
    public:
        //--- constructor ---
        buttonCommands (
                gpio_evaluatedSwitch * button_f,
                controlledArmchair * control_f,
                buzzer_t * buzzer_f,
                controlledMotor * motorLeft_f, 
                controlledMotor * motorRight_f
                ); 

        //--- functions ---
        //the following function has to be started once in a separate task. 
        //repeatedly evaluates and processes button events then takes the corresponding action
        void startHandleLoop(); 

    private:
        //--- functions ---
        void action(uint8_t count);

        //--- objects ---
        gpio_evaluatedSwitch* button;
        controlledArmchair * control;
        buzzer_t* buzzer;
        controlledMotor * motorLeft;
        controlledMotor * motorRight;

        //--- variables ---
        uint8_t count = 0;
        uint32_t timestamp_lastAction = 0;
        enum class inputState_t {IDLE, WAIT_FOR_INPUT};
        inputState_t state = inputState_t::IDLE;

};

