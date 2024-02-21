extern "C" {
#include "hal/timer_types.h"
}

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
evaluatedJoystick::evaluatedJoystick(joystick_config_t config_f, nvs_handle_t * nvsHandle_f){
    config = config_f;
    nvsHandle = nvsHandle_f;
    init();
}



//----------------------------
//---------- init ------------
//----------------------------
void evaluatedJoystick::init(){
    ESP_LOGW(TAG, "initializing ADC's and loading calibration...");
    //initialize adc
    adc1_config_width(ADC_WIDTH_BIT_12); //=> max resolution 4096
                                         
    //FIXME: the following two commands each throw error 
    //"ADC: adc1_lock_release(419): adc1 lock release called before acquire"
    //note: also happens for each get_raw for first call of readAdc function
    //when run in main function that does not happen -> move init from constructor to be called in main
    adc1_config_channel_atten(config.adc_x, ADC_ATTEN_DB_11); //max voltage
    adc1_config_channel_atten(config.adc_y, ADC_ATTEN_DB_11); //max voltage

    //load stored calibration values (if not found loads defaults from config)
    loadCalibration(X_MIN);
    loadCalibration(X_MAX);
    loadCalibration(Y_MIN);
    loadCalibration(Y_MAX);

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
		ets_delay_us(50);
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
	//TODO duplicate code for each axis below:
    ESP_LOGV(TAG, "getting X coodrdinate...");
	uint32_t adcRead;
	adcRead = readAdc(config.adc_x, config.x_inverted);
    float x = scaleCoordinate(readAdc(config.adc_x, config.x_inverted), x_min, x_max, x_center,  config.tolerance_zeroX_per, config.tolerance_end_per);
    data.x = x;
	ESP_LOGD(TAG, "X: adc-raw=%d \tadc-conv=%d \tmin=%d \t max=%d \tcenter=%d \tinverted=%d => x=%.3f",
        adc1_get_raw(config.adc_x), adcRead,  x_min, x_max, x_center, config.x_inverted, x);

    ESP_LOGV(TAG, "getting Y coodrinate...");
	adcRead = readAdc(config.adc_y, config.y_inverted);
    float y = scaleCoordinate(adcRead, y_min, y_max, y_center,  config.tolerance_zeroY_per, config.tolerance_end_per);
    data.y = y;
	ESP_LOGD(TAG, "Y: adc-raw=%d \tadc-conv=%d \tmin=%d \t max=%d \tcenter=%d \tinverted=%d => y=%.3lf",
        adc1_get_raw(config.adc_y), adcRead,  y_min, y_max, y_center, config.y_inverted, y);

    //calculate radius
    data.radius = sqrt(pow(data.x,2) + pow(data.y,2));
    if (data.radius > 1-config.tolerance_radius) {
        data.radius = 1;
    }

    //calculate angle
    data.angle = (atan(data.y/data.x) * 180) / 3.141;

    //define position
    data.position = joystick_evaluatePosition(x, y);

	ESP_LOGD(TAG, "X=%.2f  Y=%.2f  radius=%.2f  angle=%.2f", data.x, data.y, data.radius, data.angle);
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

    ESP_LOGD(TAG, "scaling: in=%.3f coordinate=%.3f, tolZero=%.3f, tolEnd=%.3f", input, coordinate, tolerance_zero, tolerance_end);
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
//TODO rename this function to more general name (scales not only coordinates e.g. adjusts radius, in future angle...)
void joystick_scaleCoordinatesLinear(joystickData_t * data, float pointX, float pointY){
    // --- scale x and y coordinate --- DISABLED
	/*
    data->x = scaleLinPoint(data->x, pointX, pointY);
    data->y = scaleLinPoint(data->y, pointX, pointY);
    //re-calculate radius
    data->radius = sqrt(pow(data->x,2) + pow(data->y,2));
    if (data->radius > 1-0.1) {//FIXME hardcoded radius tolerance
        data->radius = 1;
    }
	*/
	
	//note: issue with scaling X, Y coordinates:
	//  - messed up radius calculation - radius never gets 1 at diagonal positions
	//==> only scaling radius as only speed should be more acurate at low radius:
	//TODO make that clear and rename function, since it does not scale coordinates - just radius
	
	//--- scale radius ---
	data-> radius = scaleLinPoint(data->radius, pointX, pointY);
}




//=============================================
//========= joystick_evaluatePosition =========
//=============================================
//function that defines and returns enum joystickPos from x and y coordinates
joystickPos_t joystick_evaluatePosition(float x, float y){
    //define position
    //--- center ---
    if((fabs(x) == 0) && (fabs(y) == 0)){ 
        return joystickPos_t::CENTER;
    }
    //--- x axis ---
    else if(fabs(y) == 0){
        return joystickPos_t::X_AXIS;
    }
    //--- y axis ---
    else if(fabs(x) == 0){
        return joystickPos_t::Y_AXIS;
    }
    //--- top right ---
    else if(x > 0 && y > 0){
        return joystickPos_t::TOP_RIGHT;
    }
    //--- top left ---
    else if(x < 0 && y > 0){
        return joystickPos_t::TOP_LEFT;
    }
    //--- bottom left ---
    else if(x < 0 && y < 0){
        return joystickPos_t::BOTTOM_LEFT;
    }
    //--- bottom right ---
    else if(x > 0 && y < 0){
        return joystickPos_t::BOTTOM_RIGHT;
    }
    //--- other ---
    else {
        return joystickPos_t::CENTER;
    }

}




//============================================
//========= joystick_CommandsDriving =========
//============================================
//function that generates commands for both motors from the joystick data
motorCommands_t joystick_generateCommandsDriving(joystickData_t data, bool altStickMapping){

    //struct with current data of the joystick
    //typedef struct joystickData_t {
    //    joystickPos_t position;
    //    float x;
    //    float y;
    //    float radius;
    //    float angle;
    //} joystickData_t;

	//--- variables ---
    motorCommands_t commands;
    float dutyMax = 100; //TODO add this to config, make changeable during runtime

    float dutyOffset = 5; //immediately starts with this duty, TODO add this to config
    float dutyRange = dutyMax - dutyOffset;
    float ratio = fabs(data.angle) / 90; //90degree = x=0 || 0degree = y=0
	
	//--- snap ratio to max at angle threshold --- 
	//(-> more joystick area where inner wheel is off when turning)
	/*
	//FIXME works, but armchair unsusable because of current bug with motor driver (inner motor freezes after turn)
	float ratioClipThreshold = 0.3;
	if (ratio < ratioClipThreshold) ratio = 0;
	else if (ratio > 1-ratioClipThreshold) ratio = 1;
	//TODO subtract this clip threshold from available joystick range at ratio usage
	*/

    //--- experimental alternative control mode ---
    if (altStickMapping == true){
        //swap BOTTOM_LEFT and BOTTOM_RIGHT
        if (data.position == joystickPos_t::BOTTOM_LEFT){
            data.position = joystickPos_t::BOTTOM_RIGHT;
        }
        else if (data.position == joystickPos_t::BOTTOM_RIGHT){
            data.position = joystickPos_t::BOTTOM_LEFT;
        }
    }

	//--- handle all positions ---
	//define target direction and duty according to position
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
            commands.right.duty = data.radius * dutyRange - (data.radius*dutyRange + dutyOffset)*(1-ratio) + dutyOffset;
            break;

        case joystickPos_t::TOP_LEFT:
            commands.left.state = motorstate_t::FWD;
            commands.right.state = motorstate_t::FWD;
            commands.left.duty =  data.radius * dutyRange - (data.radius*dutyRange + dutyOffset)*(1-ratio) + dutyOffset;
            commands.right.duty = data.radius * dutyRange + dutyOffset;
            break;

        case joystickPos_t::BOTTOM_LEFT:
            commands.left.state = motorstate_t::REV;
            commands.right.state = motorstate_t::REV;
            commands.left.duty = data.radius * dutyRange + dutyOffset;
            commands.right.duty = data.radius * dutyRange - (data.radius*dutyRange + dutyOffset)*(1-ratio) + dutyOffset;
            break;

        case joystickPos_t::BOTTOM_RIGHT:
            commands.left.state = motorstate_t::REV;
            commands.right.state = motorstate_t::REV;
            commands.left.duty = data.radius * dutyRange - (data.radius*dutyRange + dutyOffset)*(1-ratio) + dutyOffset;
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
joystickPos_t lastStickPos = joystickPos_t::CENTER;
//stick position quadrant only with "X_AXIS and Y_AXIS" as hysteresis
joystickPos_t stickQuadrant = joystickPos_t::CENTER;

//--- configure shake mode --- TODO: move this to config
uint32_t shake_msOffMax = 80;
uint32_t shake_msOnMax = 120;
float dutyShake = 60;

//function that generates commands for both motors from the joystick data
motorCommands_t joystick_generateCommandsShaking(joystickData_t data){

    //--- handle pulsing shake variable ---
    //TODO remove this, make individual per mode?
    //TODO only run this when not CENTER anyways?
    motorCommands_t commands;
    float ratio = fabs(data.angle) / 90; //90degree = x=0 || 0degree = y=0

    //calculate on/off duration
    uint32_t msOn = shake_msOnMax * data.radius;
    uint32_t msOff = shake_msOffMax * data.radius;

    //evaluate state (on/off)
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

    //struct with current data of the joystick
    //typedef struct joystickData_t {
    //    joystickPos_t position;
    //    float x;
    //    float y;
    //    float radius;
    //    float angle;
    //} joystickData_t;

    //--- evaluate stick position --- 
    //4 quadrants and center only - with X and Y axis as hysteresis
    switch (data.position){

        case joystickPos_t::CENTER:
            //immediately set to center at center
            stickQuadrant = joystickPos_t::CENTER;
            break;

        case joystickPos_t::Y_AXIS:
            //when moving from center to axis initially start in a certain quadrant
            if (stickQuadrant == joystickPos_t::CENTER) {
                if (data.y > 0){
                    stickQuadrant = joystickPos_t::TOP_RIGHT;
                } else {
                    stickQuadrant = joystickPos_t::BOTTOM_RIGHT;
                }
            }
            break;

        case joystickPos_t::X_AXIS:
            //when moving from center to axis initially start in a certain quadrant
            if (stickQuadrant == joystickPos_t::CENTER) {
                if (data.x > 0){
                    stickQuadrant = joystickPos_t::TOP_RIGHT;
                } else {
                    stickQuadrant = joystickPos_t::TOP_LEFT;
                }
            }
            break;

        case joystickPos_t::TOP_RIGHT:
        case joystickPos_t::TOP_LEFT:
        case joystickPos_t::BOTTOM_LEFT:
        case joystickPos_t::BOTTOM_RIGHT:
            //update/change evaluated pos when in one of the 4 quadrants
            stickQuadrant = data.position;
            //TODO: maybe beep when switching mode? (difficult because beep object has to be passed to function)
            break;
    }


    //--- handle different modes (joystick in any of 4 quadrants) ---
    switch (stickQuadrant){
        case joystickPos_t::CENTER:
        case joystickPos_t::X_AXIS: //never true
        case joystickPos_t::Y_AXIS: //never true
            commands.left.state = motorstate_t::IDLE;
            commands.right.state = motorstate_t::IDLE;
            commands.left.duty = 0;
            commands.right.duty = 0;
            ESP_LOGI(TAG_CMD, "generate shake commands: CENTER -> idle");
            return commands;
            break;
            //4 different modes
        case joystickPos_t::TOP_RIGHT:
            commands.left.state = motorstate_t::FWD;
            commands.right.state = motorstate_t::FWD;
            break;
        case joystickPos_t::TOP_LEFT:
            commands.left.state = motorstate_t::REV;
            commands.right.state = motorstate_t::REV;
            break;
        case joystickPos_t::BOTTOM_LEFT:
            commands.left.state = motorstate_t::REV;
            commands.right.state = motorstate_t::FWD;
            break;
        case joystickPos_t::BOTTOM_RIGHT:
            commands.left.state = motorstate_t::FWD;
            commands.right.state = motorstate_t::REV;
            break;
    }


    //--- turn motors on/off depending on pulsing shake variable ---
    if (shake_state == true){
        //set duty to shake
        commands.left.duty = dutyShake;
        commands.right.duty = dutyShake;
        //directions are defined above depending on mode
    } else {
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




// corresponding storage key strings to each joystickCalibratenMode variable
const char *calibrationStorageKeys[] = {"stick_x-min", "stick_x-max", "stick_y-min", "stick_y-max", "", ""};

//-------------------------------
//------- loadCalibration -------
//-------------------------------
// loads selected calibration value from nvs or default values from config if no data stored
void evaluatedJoystick::loadCalibration(joystickCalibrationMode_t mode)
{
    // determine desired variables
    int *configValue, *usedValue;
    switch (mode)
    {
    case X_MIN:
        configValue = &(config.x_min);
        usedValue = &x_min;
        break;
    case X_MAX:
        configValue = &(config.x_max);
        usedValue = &x_max;
        break;
    case Y_MIN:
        configValue = &(config.y_min);
        usedValue = &y_min;
        break;
    case Y_MAX:
        configValue = &(config.y_max);
        usedValue = &y_max;
        break;
    case X_CENTER:
    case Y_CENTER:
    default:
        // center position is not stored in nvs, it gets defined at startup or during calibration
        ESP_LOGE(TAG, "loadCalibration: 'center_x' and 'center_y' are not stored in nvs -> not assigning anything");
        // defineCenter();
        return; 
    }

    // read from nvs
    int16_t valueRead;
    esp_err_t err = nvs_get_i16(*nvsHandle, calibrationStorageKeys[(int)mode], &valueRead);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGW(TAG, "Successfully read value '%s' from nvs. Overriding default value %d with %d", calibrationStorageKeys[(int)mode], *configValue, valueRead);
        *usedValue = (int)valueRead;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "nvs: the value '%s' is not initialized yet, loading default value %d", calibrationStorageKeys[(int)mode], *configValue);
        *usedValue = *configValue;
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading nvs!", esp_err_to_name(err));
        *usedValue = *configValue;
    }
}



//-------------------------------
//------- loadCalibration -------
//-------------------------------
// loads selected calibration value from nvs or default values from config if no data stored
void evaluatedJoystick::writeCalibration(joystickCalibrationMode_t mode, int newValue)
{
    // determine desired variables
    int *configValue, *usedValue;
    switch (mode)
    {
    case X_MIN:
        configValue = &(config.x_min);
        usedValue = &x_min;
        break;
    case X_MAX:
        configValue = &(config.x_max);
        usedValue = &x_max;
        break;
    case Y_MIN:
        configValue = &(config.y_min);
        usedValue = &y_min;
        break;
    case Y_MAX:
        configValue = &(config.y_max);
        usedValue = &y_max;
        break;
    case X_CENTER:
        x_center = newValue;
        ESP_LOGW(TAG, "writeCalibration: 'center_x' or 'center_y' are not stored in nvs -> loading only");
        return;
    case Y_CENTER:
        y_center = newValue;
        ESP_LOGW(TAG, "writeCalibration: 'center_x' or 'center_y' are not stored in nvs -> loading only");
    default:
        return;
    }

    // check if unchanged
    if (*usedValue == newValue)
    {
        ESP_LOGW(TAG, "writeCalibration: value '%s' unchanged at %d, not writing to nvs", calibrationStorageKeys[(int)mode], newValue);
        return;
    }

    // update nvs value
    ESP_LOGW(TAG, "writeCalibration: updating nvs value '%s' from %d to %d", calibrationStorageKeys[(int)mode], *usedValue, newValue);
    esp_err_t err = nvs_set_i16(*nvsHandle, calibrationStorageKeys[(int)mode], newValue);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed writing");
    err = nvs_commit(*nvsHandle);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed committing updates");
    else
        ESP_LOGI(TAG, "nvs: successfully committed updates");
    // update variable
    *usedValue = newValue;
}