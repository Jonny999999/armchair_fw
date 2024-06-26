#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdbool.h>
}

#include <cmath>
#include "types.hpp"


//======================================
//========= evaluated Joystick =========
//======================================
//class which evaluates a joystick with 2 analog signals
// - scales the adc input to coordinates with detailed tolerances
// - calculates angle and radius
// - defines an enum with position information

//--------------------------------------------
//---- struct, enum, variable declarations ---
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

typedef enum joystickCalibrationMode_t { X_MIN = 0, X_MAX, Y_MIN, Y_MAX, X_CENTER, Y_CENTER } joystickCalibrationMode_t;

//struct with current data of the joystick
typedef struct joystickData_t {
    joystickPos_t position;
    float x;
    float y;
    float radius;
    float angle;
} joystickData_t;

// struct with parameters provided to joystick_GenerateCommandsDriving()
typedef struct joystickGenerateCommands_config_t
{
    float maxDutyStraight;  // max duty applied when driving with ratio=1 (when turning it might increase by Boost)
    float maxRelativeBoostPercentOfMaxDuty; // max duty percent added to outer tire when turning (max actual is 100-maxDutyStraight) - set 0 to disable
    // note: to be able to reduce the overall driving speed boost has to be limited as well otherwise outer tire when turning would always be 100% no matter of maxDuty
    float dutyOffset;              // motors immediately start with this duty (duty movement starts)
    float ratioSnapToOneThreshold; // have some area around X-Axis where inner tire is completely off - set 1 to disable
    bool altStickMapping;          // swap reverse direction
} joystickGenerateCommands_config_t;

//------------------------------------
//----- evaluatedJoystick class  -----
//------------------------------------
class evaluatedJoystick
{
public:
    //--- constructor ---
    evaluatedJoystick(joystick_config_t config_f, nvs_handle_t * nvsHandle);

    //--- functions ---
    joystickData_t getData(); // read joystick, calculate values and return the data in a struct
    // get raw adc value (inversion applied)
    int getRawX() { return readAdc(config.adc_x, config.x_inverted); }
    int getRawY() { return readAdc(config.adc_y, config.y_inverted); }
    void defineCenter(); // define joystick center from current position
    void writeCalibration(joystickCalibrationMode_t mode, int newValue); // load certain new calibration value and store it in nvs

private:
    //--- functions ---
    // initialize adc inputs, define center
    void init();
    // loads selected calibration value from nvs or default values from config if no data stored
    void loadCalibration(joystickCalibrationMode_t mode);
        // read adc while making multiple samples with option to invert the result
        int readAdc(adc1_channel_t adc_channel, bool inverted = false);

        //--- variables ---
        // handle for using the nvs flash (persistent config variables)
        nvs_handle_t *nvsHandle;
        joystick_config_t config;

        int x_min;
        int x_max;
        int y_min;
        int y_max;
        int x_center;
        int y_center;

        joystickData_t data;
        float x;
        float y;
    };



//============================================
//========= joystick_CommandsDriving =========
//============================================
//function that generates commands for both motors from the joystick data
//motorCommands_t joystick_generateCommandsDriving(evaluatedJoystick joystick);
motorCommands_t joystick_generateCommandsDriving(joystickData_t data, joystickGenerateCommands_config_t * config);



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
joystickPos_t joystick_evaluatePosition(float x, float y);
