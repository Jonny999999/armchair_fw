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






//============================================
//========= joystick_CommandsDriving =========
//============================================
//function that generates commands for both motors from the joystick data
motorCommands_t joystick_generateCommandsDriving(evaluatedJoystick joystick){


    //struct with current data of the joystick
    //typedef struct joystickData_t {
    //    joystickPos_t position;
    //    float x;
    //    float y;
    //    float radius;
    //    float angle;
    //} joystickData_t;


    joystickData_t data = joystick.getData();
    motorCommands_t commands;
    float dutyMax = 60; //TODO add this to config, make changeable during runtime

    float dutyOffset = 5; //immedeately starts with this duty, TODO add this to config
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
