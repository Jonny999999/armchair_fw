#include "joystick.hpp"


//definition of string array to be able to convert state enum to readable string
const char* joystickPosStr[7] = {"CENTER", "Y_AXIS", "X_AXIS", "TOP_RIGHT", "TOP_LEFT", "BOTTOM_LEFT", "BOTTOM_RIGHT"};

//tags for logging
static const char * TAG = "evaluatedJoystick";
static const char * TAG_CMD = "joystickCommands";




//-----------------------------
//-------- constructor --------
//-----------------------------
//copy provided struct with all configuration and run init function
evaluatedJoystick::evaluatedJoystick(joystick_config_t config_f){
    config = config_f;
    init();
}




//----------------------------
//---------- init ------------
//----------------------------
void evaluatedJoystick::init(){
    ESP_LOGI(TAG, "initializing joystick");
    //initialize adc
    adc1_config_width(ADC_WIDTH_BIT_12); //=> max resolution 4096
                                         
    //FIXME: the following two commands each throw error 
    //"ADC: adc1_lock_release(419): adc1 lock release called before acquire"
    //note: also happens for each get_raw for first call of readAdc function
    //when run in main function that does not happen
    adc1_config_channel_atten(config.adc_x, ADC_ATTEN_DB_11); //max voltage
    adc1_config_channel_atten(config.adc_y, ADC_ATTEN_DB_11); //max voltage

    //define joystick center from current position
    defineCenter(); //define joystick center from current position
}




//-----------------------------
//--------- readAdc -----------
//-----------------------------
//function for multisampling an anlog input
int evaluatedJoystick::readAdc(adc1_channel_t adc_channel, bool inverted) {
    //make multiple measurements
    int adc_reading = 0;
    for (int i = 0; i < 16; i++) {
        adc_reading += adc1_get_raw(adc_channel);
    }
    adc_reading = adc_reading / 16;

    //return original or inverted result
    if (inverted) {
        return 4095 - adc_reading;
    } else {
        return adc_reading;
    }
}




//-------------------------------
//---------- getData ------------
//-------------------------------
//function that reads the joystick, calculates values and returns a struct with current data
joystickData_t evaluatedJoystick::getData() {
    //get coordinates
    //TODO individual tolerances for each axis? Otherwise some parameters can be removed
    ESP_LOGD(TAG, "getting X coodrinate...");
    float x = scaleCoordinate(readAdc(config.adc_x, config.x_inverted), config.x_min, config.x_max, x_center,  config.tolerance_zeroX_per, config.tolerance_end_per);
    data.x = x;

    ESP_LOGD(TAG, "getting Y coodrinate...");
    float y = scaleCoordinate(readAdc(config.adc_y, config.y_inverted), config.y_min, config.y_max, y_center,  config.tolerance_zeroY_per, config.tolerance_end_per);
    data.y = y;

    //calculate radius
    data.radius = sqrt(pow(data.x,2) + pow(data.y,2));
    if (data.radius > 1-config.tolerance_radius) {
        data.radius = 1;
    }

    //calculate angle
    data.angle = (atan(data.y/data.x) * 180) / 3.141;

    //define position
    data.position = joystick_evaluatePosition(x, y, &stickPosPrevious);

    return data;
}




//----------------------------
//------ defineCenter --------
//----------------------------
//function that defines the current position of the joystick as center position
void evaluatedJoystick::defineCenter(){
    //read voltage from adc
    x_center = readAdc(config.adc_x, config.x_inverted);
    y_center = readAdc(config.adc_y, config.y_inverted);

    ESP_LOGW(TAG, "defined center to x=%d, y=%d", x_center, y_center);
}






//==============================
//====== scaleCoordinate =======
//==============================
//function that scales an input value (e.g. from adc pin) to a value from -1 to 1 using the given thresholds and tolerances
float scaleCoordinate(float input, float min, float max, float center, float tolerance_zero_per, float tolerance_end_per) {

    float coordinate = 0;

    //convert tolerance percentages to actual values of range
    double tolerance_zero = (max-min) * tolerance_zero_per / 100;
    double tolerance_end = (max-min) * tolerance_end_per / 100;

    //define coordinate value considering the different tolerances
    //--- center ---
    if ((input < center+tolerance_zero) && (input > center-tolerance_zero) ) { //adc value is inside tolerance around center threshold
        coordinate = 0;
    }
    //--- maximum ---
    else if (input > max-tolerance_end) {
        coordinate = 1;
    }
    //--- minimum ---
    else if (input < min+tolerance_end) {
        coordinate = -1;
    }
    //--- positive area ---
    else if (input > center) {
        float range = max - center - tolerance_zero - tolerance_end;
        coordinate = (input - center - tolerance_zero) / range;
    }
    //--- negative area ---
    else if (input < center) {
        float range = (center - min - tolerance_zero - tolerance_end);
        coordinate = -(center-input - tolerance_zero) / range;
    }

    ESP_LOGD(TAG, "scaled coordinate from %.3f to %.3f, tolZero=%.3f, tolEnd=%.3f", input, coordinate, tolerance_zero, tolerance_end);
    //return coordinate (-1 to 1)
    return coordinate;

}




//===========================================
//====== joystick_scaleCoordinatesExp =======
//===========================================
//local function that scales the absolute value of a variable exponentionally
float scaleExp(float value, float exponent){
    float result = powf(fabs(value), exponent);
    if (value >= 0) {
        return result;
    } else {
        return -result;
    }
}
//function that updates a joystickData object with exponentionally scaling applied to coordinates
void joystick_scaleCoordinatesExp(joystickData_t * data, float exponent){
    //scale x and y coordinate
    data->x = scaleExp(data->x, exponent);
    data->y = scaleExp(data->y, exponent);
    //re-calculate radius
    data->radius = sqrt(pow(data->x,2) + pow(data->y,2));
    if (data->radius > 1-0.07) {//FIXME hardcoded radius tolerance
        data->radius = 1;
    }
}




//==============================================
//====== joystick_scaleCoordinatesLinear =======
//==============================================
//local function that scales value from -1-1 to -1-1 with two different slopes before and after a specified point
//slope1: for value from 0 to pointX -> scale linear from  0 to pointY
//slope2: for value from pointX to 1 -> scale linear from pointY to 1
float scaleLinPoint(float value, float pointX, float pointY){
    float result;
    if (fabs(value) <= pointX) {
        //--- scale on line from 0 to point ---
        result = fabs(value) * (pointY/pointX);
    } else {
        //--- scale on line from point to 1 ---
        float m = (1-pointY) / (1-pointX);
        result = fabs(value) * m + (1 - m);
    }

    //--- return result with same sign as input ---
    if (value >= 0) {
        return result;
    } else {
        return -result;
    }
}
//function that updates a joystickData object with linear scaling applied to coordinates
//e.g. use to use more joystick resolution for lower speeds
void joystick_scaleCoordinatesLinear(joystickData_t * data, float pointX, float pointY){
    //scale x and y coordinate
    data->x = scaleLinPoint(data->x, pointX, pointY);
    data->y = scaleLinPoint(data->y, pointX, pointY);
    //re-calculate radius
    data->radius = sqrt(pow(data->x,2) + pow(data->y,2));
    if (data->radius > 1-0.07) {//FIXME hardcoded radius tolerance
        data->radius = 1;
    }
}




//=============================================
//========= joystick_evaluatePosition =========
//=============================================
//function that defines and returns enum joystickPos from x and y coordinates
joystickPos_t joystick_evaluatePosition(float x, float y, joystickPos_t* prevPos){
    joystickPos_t newPos;
    //define position
    //--- center ---
    if((fabs(x) == 0) && (fabs(y) == 0)){ 
        newPos = joystickPos_t::CENTER;
    }
    //--- x axis ---
    else if(fabs(y) == 0){
        newPos = joystickPos_t::X_AXIS;
    }
    //--- y axis ---
    else if(fabs(x) == 0){
        newPos = joystickPos_t::Y_AXIS;
    }
    //--- top right ---
    else if(x > 0 && y > 0){
        newPos = joystickPos_t::TOP_RIGHT;
    }
    //--- top left ---
    else if(x < 0 && y > 0){
        newPos = joystickPos_t::TOP_LEFT;
    }
    //--- bottom left ---
    else if(x < 0 && y < 0){
        newPos = joystickPos_t::BOTTOM_LEFT;
    }
    //--- bottom right ---
    else if(x > 0 && y < 0){
        newPos = joystickPos_t::BOTTOM_RIGHT;
    }
    //--- other ---
    else {
        newPos = joystickPos_t::CENTER;
    }



    //--- apply hysteresis on change from X-AXIS ---
    //=> prevent frequent switching between X-AXIS and other positions
    //otherwise this results in a very jerky motor action when in driving mode
    float xAxisHyst = 0.2;

    //change in position
    if (newPos != *prevPos) {

        //switched FROM x-axis to other position
        if(*prevPos == joystickPos_t::X_AXIS) { //switch FROM X_AXIS

            //verify coordinate changed more than hysteresis
            if (fabs(y) < xAxisHyst) { //less offset than hysteresis
                newPos = joystickPos_t::X_AXIS; //stay at X_AXIS position
            } else { //switch is valid (enough change)
                *prevPos = newPos;
            }

        } else { //switched to any other position
                *prevPos = newPos;
        }
    }
    return newPos;
}




//============================================
//========= joystick_CommandsDriving =========
//============================================
//function that generates commands for both motors from the joystick data
motorCommands_t joystick_generateCommandsDriving(joystickData_t data){


    //struct with current data of the joystick
    //typedef struct joystickData_t {
    //    joystickPos_t position;
    //    float x;
    //    float y;
    //    float radius;
    //    float angle;
    //} joystickData_t;


    motorCommands_t commands;
    float dutyMax = 94; //TODO add this to config, make changeable during runtime

    float dutyOffset = 10; //immedeately starts with this duty, TODO add this to config
    float dutyRange = dutyMax - dutyOffset;
    float ratio = fabs(data.angle) / 90; //90degree = x=0 || 0degree = y=0

    switch (data.position){

        case joystickPos_t::CENTER:
            commands.left.state = motorstate_t::IDLE;
            commands.right.state = motorstate_t::IDLE;
            commands.left.duty = 0;
            commands.right.duty = 0;
            break;

        case joystickPos_t::Y_AXIS:
            if (data.y > 0){
                commands.left.state = motorstate_t::FWD;
                commands.right.state = motorstate_t::FWD;
            } else {
                commands.left.state = motorstate_t::REV;
                commands.right.state = motorstate_t::REV;
            }
            commands.left.duty = fabs(data.y) * dutyRange + dutyOffset;
            commands.right.duty = commands.left.duty;
            break;

        case joystickPos_t::X_AXIS:
            if (data.x > 0) {
                commands.left.state = motorstate_t::FWD;
                commands.right.state = motorstate_t::REV;
            } else {
                commands.left.state = motorstate_t::REV;
                commands.right.state = motorstate_t::FWD;
            }
            commands.left.duty = fabs(data.x) * dutyRange + dutyOffset;
            commands.right.duty = commands.left.duty;
            break;

        case joystickPos_t::TOP_RIGHT:
            commands.left.state = motorstate_t::FWD;
            commands.right.state = motorstate_t::FWD;
            commands.left.duty = data.radius * dutyRange + dutyOffset;
            commands.right.duty = data.radius * dutyRange - data.radius*dutyRange*(1-ratio) + dutyOffset;
            break;

        case joystickPos_t::TOP_LEFT:
            commands.left.state = motorstate_t::FWD;
            commands.right.state = motorstate_t::FWD;
            commands.left.duty =  data.radius * dutyRange - data.radius*dutyRange*(1-ratio) + dutyOffset;
            commands.right.duty = data.radius * dutyRange + dutyOffset;
            break;

        case joystickPos_t::BOTTOM_LEFT:
            commands.left.state = motorstate_t::REV;
            commands.right.state = motorstate_t::REV;
            commands.left.duty = data.radius * dutyRange + dutyOffset;
            commands.right.duty = data.radius * dutyRange - data.radius*dutyRange*(1-ratio) + dutyOffset; //TODO remove offset? allow one motor only
            break;

        case joystickPos_t::BOTTOM_RIGHT:
            commands.left.state = motorstate_t::REV;
            commands.right.state = motorstate_t::REV;
            commands.left.duty = data.radius * dutyRange - data.radius*dutyRange*(1-ratio) + dutyOffset; //TODO remove offset? allow one motor only
            commands.right.duty =  data.radius * dutyRange + dutyOffset;
            break;
    }

    ESP_LOGI(TAG_CMD, "generated commands from data: state=%s, angle=%.3f, ratio=%.3f/%.3f, radius=%.2f, x=%.2f, y=%.2f",
            joystickPosStr[(int)data.position], data.angle, ratio, (1-ratio), data.radius, data.x, data.y);
    ESP_LOGI(TAG_CMD, "motor left: state=%s, duty=%.3f", motorstateStr[(int)commands.left.state], commands.left.duty);
    ESP_LOGI(TAG_CMD, "motor right: state=%s, duty=%.3f", motorstateStr[(int)commands.right.state], commands.right.duty);
    return commands;
}



//============================================
//========= joystick_CommandsShaking =========
//============================================
//--- variable declarations ---
uint32_t shake_timestamp_turnedOn = 0;
uint32_t shake_timestamp_turnedOff = 0;
bool shake_state = false;

//--- configure shake mode --- TODO: move this to config
uint32_t shake_msOffMax = 90;
uint32_t shake_msOnMax = 180;
float dutyShake = 60;

//function that generates commands for both motors from the joystick data
motorCommands_t joystick_generateCommandsShaking(joystickData_t data){


    //struct with current data of the joystick
    //typedef struct joystickData_t {
    //    joystickPos_t position;
    //    float x;
    //    float y;
    //    float radius;
    //    float angle;
    //} joystickData_t;


    motorCommands_t commands;
    float ratio = fabs(data.angle) / 90; //90degree = x=0 || 0degree = y=0

    //--- calculate on/off duration ---
    uint32_t msOn = shake_msOnMax * data.radius;
    uint32_t msOff = shake_msOffMax * data.radius;

    //--- evaluate state (on/off) ---
    if (data.radius > 0 ){
        //currently off
        if (shake_state == false){
            //off long enough
            if (esp_log_timestamp() - shake_timestamp_turnedOff > msOff) {
                //turn on
                shake_state = true;
                shake_timestamp_turnedOn = esp_log_timestamp();
            }
        } 
        //currently on
        else {
            //on long enough
            if (esp_log_timestamp() - shake_timestamp_turnedOn > msOn) {
                //turn off
                shake_state = false;
                shake_timestamp_turnedOff = esp_log_timestamp();
            }
        }
    } 
    //joystick is at center
    else {
        shake_state = false;
        shake_timestamp_turnedOff = esp_log_timestamp();
    }





    if (shake_state){
        switch (data.position){

            default:
                commands.left.state = motorstate_t::IDLE;
                commands.right.state = motorstate_t::IDLE;
                commands.left.duty = 0;
                commands.right.duty = 0;
                break;

            case joystickPos_t::Y_AXIS:
                if (data.y > 0){
                    commands.left.state = motorstate_t::FWD;
                    commands.right.state = motorstate_t::FWD;
                } else {
                    commands.left.state = motorstate_t::REV;
                    commands.right.state = motorstate_t::REV;
                }
                //set duty to shake
                commands.left.duty = dutyShake;
                commands.right.duty = dutyShake;
                break;

            case joystickPos_t::X_AXIS:
                if (data.x > 0) {
                    commands.left.state = motorstate_t::FWD;
                    commands.right.state = motorstate_t::REV;
                } else {
                    commands.left.state = motorstate_t::REV;
                    commands.right.state = motorstate_t::FWD;
                }
                //set duty to shake
                commands.left.duty = dutyShake;
                commands.right.duty = dutyShake;
                break;
        }
    } else { //shake state off
        commands.left.state = motorstate_t::IDLE;
        commands.right.state = motorstate_t::IDLE;
        commands.left.duty = 0;
        commands.right.duty = 0;
    }


    ESP_LOGI(TAG_CMD, "generated commands from data: state=%s, angle=%.3f, ratio=%.3f/%.3f, radius=%.2f, x=%.2f, y=%.2f",
            joystickPosStr[(int)data.position], data.angle, ratio, (1-ratio), data.radius, data.x, data.y);
    ESP_LOGI(TAG_CMD, "motor left: state=%s, duty=%.3f", motorstateStr[(int)commands.left.state], commands.left.duty);
    ESP_LOGI(TAG_CMD, "motor right: state=%s, duty=%.3f", motorstateStr[(int)commands.right.state], commands.right.duty);
    return commands;
}
