#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_err.h"
}

#include <cmath>
#include "motorctl.hpp" //for deklaration of motorCommands_t struct


//======================================
//========= evaluated Joystick =========
//======================================
//class which evaluates a joystick with 2 analog signals
// - scales the adc input to coordinates with detailed tolerances
// - calculates angle and radius
// - defines an enum with position information

//--------------------------------------------
//---- struct, enum, variable deklarations ---
//--------------------------------------------
//struct with all required configuration parameters
typedef struct joystick_config_t {
    //analog inputs the axis are connected
    adc1_channel_t adc_x;
    adc1_channel_t adc_y;

    //percentage of joystick range the coordinate of the axis snaps to 0 (0-100)
    int tolerance_zeroX_per;
    int tolerance_zeroY_per;
    //percentage of joystick range the coordinate snaps to -1 or 1 before configured "_max" or "_min" threshold (mechanical end) is reached (0-100)
    int tolerance_end_per;
    //threshold the radius jumps to 1 before the stick is at max radius (range 0-1)
    float tolerance_radius;

    //min and max adc values of each axis
    int x_min;
    int x_max;
    int y_min;
    int y_max;

    //invert adc measurement (e.g. when moving joystick up results in a decreasing voltage)
    bool x_inverted;
    bool y_inverted;
} joystick_config_t;



//enum for describing the position of the joystick
enum class joystickPos_t {CENTER, Y_AXIS, X_AXIS, TOP_RIGHT, TOP_LEFT, BOTTOM_LEFT, BOTTOM_RIGHT};
extern const char* joystickPosStr[7];



//struct with current data of the joystick
typedef struct joystickData_t {
    joystickPos_t position;
    float x;
    float y;
    float radius;
    float angle;
} joystickData_t;



//------------------------------------
//----- evaluatedJoystick class  -----
//------------------------------------
class evaluatedJoystick {
    public:
        //--- constructor ---
        evaluatedJoystick(joystick_config_t config_f);

        //--- functions ---
        joystickData_t getData(); //read joystick, calculate values and return the data in a struct
        void defineCenter(); //define joystick center from current position

    private:
        //--- functions ---
        //initialize adc inputs, define center
        void init();
        //read adc while making multiple samples with option to invert the result
        int readAdc(adc1_channel_t adc_channel, bool inverted = false); 

        //--- variables ---
        joystick_config_t config;
        int x_center;
        int y_center;

        joystickData_t data;
        float x;
        float y;
        //store last joystick position for position hysteresis
        joystickPos_t stickPosPrevious = joystickPos_t::CENTER;
};





//============================================
//========= joystick_CommandsDriving =========
//============================================
//function that generates commands for both motors from the joystick data
//motorCommands_t joystick_generateCommandsDriving(evaluatedJoystick joystick);
motorCommands_t joystick_generateCommandsDriving(joystickData_t data );



//============================================
//========= joystick_CommandsShaking =========
//============================================
//function that generates commands for both motors from the joystick data
//motorCommands_t joystick_generateCommandsDriving(evaluatedJoystick joystick);
motorCommands_t joystick_generateCommandsShaking(joystickData_t data );



//==============================
//====== scaleCoordinate =======
//==============================
//function that scales an input value (e.g. from adc pin) to a value from -1 to 1 using the giben thresholds and tolerances
float scaleCoordinate(float input, float min, float max, float center, float tolerance_zero_per, float tolerance_end_per);



//===========================================
//====== joystick_scaleCoordinatesExp =======
//===========================================
//function that updates a joystickData object with exponentionally scaling applied to coordinates
//e.g. use to use more joystick resolution for lower speeds
void joystick_scaleCoordinatesExp(joystickData_t * data, float exponent);



//==============================================
//====== joystick_scaleCoordinatesLinear =======
//==============================================
//function that updates a joystickData object with linear scaling applied to coordinates
//scales coordinates with two different slopes before and after a specified point
//slope1: for value from 0 to pointX -> scale linear from  0 to pointY
//slope2: for value from pointX to 1 -> scale linear from pointY to 1
//=> best to draw the lines and point in a graph
//e.g. use to use more joystick resolution for lower speeds
void joystick_scaleCoordinatesLinear(joystickData_t * data, float pointX, float pointY);



//=============================================
//========= joystick_evaluatePosition =========
//=============================================
//function that defines and returns enum joystickPos from x and y coordinates
//joystickPos_t joystick_evaluatePosition(float x, float y);
joystickPos_t joystick_evaluatePosition(float x, float y, joystickPos_t* prevPos);
