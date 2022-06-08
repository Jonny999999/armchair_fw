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

    //range around center-threshold of each axis the coordinates stays at 0 (adc value 0-4095)
    int tolerance_zero;
    //threshold the coordinate snaps to -1 or 1 before configured "_max" or "_min" threshold (mechanical end) is reached (adc value 0-4095)
    int tolerance_end;
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
        //read input voltage and scale to value from -1 to 1 using the given thresholds and tolerances
        float getCoordinate(adc1_channel_t adc_channel, bool inverted, int min, int max, int center, int tolerance_zero, int tolerance_end);

        //--- variables ---
        joystick_config_t config;
        int x_center;
        int y_center;

        joystickData_t data;
        float x;
        float y;
};

