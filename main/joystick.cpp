#include "joystick.hpp"


//definition of string array to be able to convert state enum to readable string
const char* joystickPosStr[7] = {"CENTER", "Y_AXIS", "X_AXIS", "TOP_RIGHT", "TOP_LEFT", "BOTTOM_LEFT", "BOTTOM_RIGHT"};

//tag for logging
static const char * TAG = "evaluatedJoystick";




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
        return adc_reading;
    } else {
        return 4095 - adc_reading;
    }
}




//----------------------------
//------ getCoordinate -------
//----------------------------
//function to read voltage at a gpio pin and scale it to a value from -1 to 1 using the given thresholds and tolerances
float evaluatedJoystick::getCoordinate(adc1_channel_t adc_channel, bool inverted, int min, int max, int center, int tolerance_zero, int tolerance_end) {

    float coordinate = 0;

    //read voltage from adc
    int input = readAdc(adc_channel, inverted);

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
        coordinate = (input - center - tolerance_end) / range;
    }
    //--- negative area ---
    else if (input < center) {
        float range = (center - min - tolerance_zero - tolerance_end);
        coordinate = -(center-input - tolerance_end) / range;
    }

    ESP_LOGD(TAG, "coordinate=%.3f, input=%d/4095, isInverted=%d", coordinate, input, inverted);
    //return coordinate (-1 to 1)
    return coordinate;

}




//-------------------------------
//---------- getData ------------
//-------------------------------
//function that reads the joystick, calculates values and returns a struct with current data
joystickData_t evaluatedJoystick::getData() {
    //get coordinates
    //TODO individual tolerances for each axis? Otherwise some parameters can be removed
    ESP_LOGD(TAG, "getting X coodrinate...");
    float x = getCoordinate(config.adc_x, config.x_inverted, config.x_min, config.x_max, x_center,  config.tolerance_zero, config.tolerance_end);
    data.x = x;
    ESP_LOGD(TAG, "getting Y coodrinate...");
    float y = getCoordinate(config.adc_y, config.y_inverted, config.y_min, config.y_max, y_center,  config.tolerance_zero, config.tolerance_end);
    data.y = y;

    //calculate radius
    data.radius = sqrt(pow(data.x,2) + pow(data.y,2));
    if (data.radius > 1-config.tolerance_radius) {
        data.radius = 1;
    }

    //calculate angle
    data.angle = (atan(data.y/data.x) * 180) / 3.141;


    //define position
    //--- center ---
    if((fabs(x) == 0) && (fabs(y) == 0)){ 
        data.position = joystickPos_t::CENTER;
    }
    //--- x axis ---
    else if(fabs(y) == 0){
        data.position = joystickPos_t::X_AXIS;
    }
    //--- y axis ---
    else if(fabs(x) == 0){
        data.position = joystickPos_t::Y_AXIS;
    }
    //--- top right ---
    else if(x > 0 && y > 0){
        data.position = joystickPos_t::TOP_RIGHT;
    }
    //--- top left ---
    else if(x < 0 && y > 0){
        data.position = joystickPos_t::TOP_LEFT;
    }
    //--- bottom left ---
    else if(x < 0 && y < 0){
        data.position = joystickPos_t::BOTTOM_LEFT;
    }
    //--- bottom right ---
    else if(x > 0 && y < 0){
        data.position = joystickPos_t::BOTTOM_RIGHT;
    }


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



