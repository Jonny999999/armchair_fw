#include "motordrivers.hpp"

//TODO: move from ledc to mcpwm?
//https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/peripherals/mcpwm.html#
//https://github.com/espressif/esp-idf/tree/v4.3/examples/peripherals/mcpwm/mcpwm_basic_config

//Note fade functionality provided by LEDC would be very useful but unfortunately is not usable here because:
//"Due to hardware limitations, there is no way to stop a fade before it reaches its target duty."

//definition of string array to be able to convert state enum to readable string
const char* motorstateStr[4] = {"IDLE", "FWD", "REV", "BRAKE"};

//tag for logging
static const char * TAG = "motordriver";



//====================================
//===== single100a motor driver ======
//====================================

//-----------------------------
//-------- constructor --------
//-----------------------------
//copy provided struct with all configuration and run init function
single100a::single100a(single100a_config_t config_f){
    config = config_f;
    init();
}



//----------------------------
//---------- init ------------
//----------------------------
//function to initialize pwm output, gpio pins and calculate maxDuty
void single100a::init(){

    //--- configure ledc timer ---
    ledc_timer_config_t ledc_timer;
    ledc_timer.speed_mode       = LEDC_HIGH_SPEED_MODE;
    ledc_timer.timer_num        = config.ledc_timer;
    ledc_timer.duty_resolution  = config.resolution; //13bit gives max 5khz freq
    ledc_timer.freq_hz          = config.pwmFreq;
    ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
    //apply configuration
    ledc_timer_config(&ledc_timer);

    //--- configure ledc channel ---
    ledc_channel_config_t ledc_channel;
    ledc_channel.channel    = config.ledc_channel;
    ledc_channel.duty       = 0;
    ledc_channel.gpio_num   = config.gpio_pwm;
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.hpoint     = 0;
    ledc_channel.timer_sel  = config.ledc_timer;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.flags.output_invert = 0; //TODO: add config option to invert the pwm output?
                                          //apply configuration
    ledc_channel_config(&ledc_channel);

    //--- define gpio pins as outputs ---
    gpio_pad_select_gpio(config.gpio_a);
    gpio_set_direction(config.gpio_a, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(config.gpio_b);
    gpio_set_direction(config.gpio_b, GPIO_MODE_OUTPUT);

    //--- calculate max duty according to selected resolution ---
    dutyMax = pow(2, ledc_timer.duty_resolution) -1;
    ESP_LOGI(TAG, "initialized single100a driver");
    ESP_LOGI(TAG, "resolution=%dbit, dutyMax value=%d, resolution=%.4f %%", ledc_timer.duty_resolution, dutyMax, 100/(float)dutyMax);
}



//---------------------------
//----------- set -----------
//---------------------------
//function to put the h-bridge module in the desired state and duty cycle
void single100a::set(motorstate_t state_f, float duty_f){

    //define enabled signal state (gpio high/low) TODO: move this to constructor?
    bool enabled;
    if (config.abInverted) {
        enabled = false;
    } else {
        enabled = true;
    }

    //scale provided target duty in percent to available resolution for ledc
    uint32_t dutyScaled;
    if (duty_f > 100) { //target duty above 100%
        dutyScaled = dutyMax;
    } else if (duty_f < 0) { //target duty below 0%
        dutyScaled = 0;
    } else { //target duty 0-100%
             //scale duty to available resolution
        dutyScaled = duty_f / 100 * dutyMax;
    }

    //put the single100a h-bridge module in the desired state update duty-cycle
    switch (state_f){
        case motorstate_t::IDLE:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled); 
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
            //TODO: to fix bugged state of h-bridge module when idle and start again, maybe try to leave pwm signal on for some time before updating a/b pins?
            //no brake: (freewheel)
            gpio_set_level(config.gpio_a, enabled);
            gpio_set_level(config.gpio_b, enabled);
            break;
        case motorstate_t::BRAKE:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, 0);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
            //brake:
            gpio_set_level(config.gpio_a, !enabled);
            gpio_set_level(config.gpio_b, !enabled);
            break;
        case motorstate_t::FWD:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
            //forward:
            gpio_set_level(config.gpio_a, enabled);
            gpio_set_level(config.gpio_b, !enabled);
            break;
        case motorstate_t::REV:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled);
            ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
            //reverse:
            gpio_set_level(config.gpio_a, !enabled);
            gpio_set_level(config.gpio_b, enabled);
            break;
    }
    ESP_LOGD(TAG, "set module to state=%s, duty=%d/%d, duty_input=%.3f%%", motorstateStr[(int)state_f], dutyScaled, dutyMax, duty_f);
}
