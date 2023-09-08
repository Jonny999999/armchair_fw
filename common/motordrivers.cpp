#include "motordrivers.hpp"
#include "esp_log.h"
#include "types.hpp"

//TODO: move from ledc to mcpwm?
//https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/peripherals/mcpwm.html#
//https://github.com/espressif/esp-idf/tree/v4.3/examples/peripherals/mcpwm/mcpwm_basic_config

//Note fade functionality provided by LEDC would be very useful but unfortunately is not usable here because:
//"Due to hardware limitations, there is no way to stop a fade before it reaches its target duty."

//tag for logging
static const char * TAG = "motordriver";

//ms to wait in IDLE before BRAKE until relay actually switched
#define BRAKE_RELAY_DELAY_MS 300


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
	gpio_pad_select_gpio(config.gpio_brakeRelay);
	gpio_set_direction(config.gpio_brakeRelay, GPIO_MODE_OUTPUT);

	//--- calculate max duty according to selected resolution ---
	dutyMax = pow(2, ledc_timer.duty_resolution) -1;
	ESP_LOGW(TAG, "initialized single100a driver");
	ESP_LOGW(TAG, "resolution=%dbit, dutyMax value=%d, resolution=%.4f %%", ledc_timer.duty_resolution, dutyMax, 100/(float)dutyMax);
}



//---------------------------
//----------- set -----------
//---------------------------
//function to put the h-bridge module in the desired state and duty cycle
void single100a::set(motorCommand_t cmd){
	set(cmd.state, cmd.duty);
}

void single100a::set(motorstate_t state_f, float duty_f){

	//scale provided target duty in percent to available resolution for ledc
	uint32_t dutyScaled;
	if (duty_f > 100) { //target duty above 100%
		dutyScaled = dutyMax;
	} else if (duty_f < 0) { //target at or below 0%
		state_f = motorstate_t::IDLE;
		dutyScaled = 0;
	} else { //target duty 0-100%
			 //scale duty to available resolution
		dutyScaled = duty_f / 100 * dutyMax;
	}

	ESP_LOGV(TAG, "target-state=%s, duty=%d/%d, duty_input=%.3f%%", motorstateStr[(int)state_f], dutyScaled, dutyMax, duty_f);

	//TODO: only when previous mode was BRAKE?
	if (state_f != motorstate_t::BRAKE){
		//reset brake wait state
		brakeWaitingForRelay = false;
		//turn of brake relay
		gpio_set_level(config.gpio_brakeRelay, 0);
	}

	//put the single100a h-bridge module in the desired state update duty-cycle
	//TODO maybe outsource mode code from once switch case? e.g. idle() brake()...
	switch (state_f){
		case motorstate_t::IDLE:
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled); 
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
			//TODO: to fix bugged state of h-bridge module when idle and start again, maybe try to leave pwm signal on for some time before updating a/b pins?
			//no brake: (freewheel)
			//gpio_set_level(config.gpio_a, config.aEnabledPinState);
			//gpio_set_level(config.gpio_b, !config.bEnabledPinState);
			gpio_set_level(config.gpio_a, config.aEnabledPinState);
			gpio_set_level(config.gpio_b, config.bEnabledPinState);
			break;

		case motorstate_t::BRAKE:
			//prevent full short (no brake resistors) due to slow relay, also reduces switching load
			if (!brakeWaitingForRelay){
				ESP_LOGW(TAG, "BRAKE: turned on relay, waiting in IDLE for %d ms, then apply brake", BRAKE_RELAY_DELAY_MS);
				//switch driver to IDLE for now
				gpio_set_level(config.gpio_a, config.aEnabledPinState);
				gpio_set_level(config.gpio_b, config.bEnabledPinState);
				//start brake relay
				gpio_set_level(config.gpio_brakeRelay, 1);
				timestamp_brakeRelayPowered = esp_log_timestamp();
				brakeWaitingForRelay = true;
			}
			//check if delay for relay to switch has passed
			else if ((esp_log_timestamp() - timestamp_brakeRelayPowered) > BRAKE_RELAY_DELAY_MS) { 
				ESP_LOGD(TAG, "applying brake with brake-resistors at duty=%.2f%%", duty_f);
				//switch driver to BRAKE
				gpio_set_level(config.gpio_a, !config.aEnabledPinState);
				gpio_set_level(config.gpio_b, !config.bEnabledPinState);
				//apply duty
				//FIXME switch between BREAK and IDLE with PWM and ignore pwm-pin? (needs test)
				ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled);
				ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
			} else {
				//waiting... stay in IDLE
				ESP_LOGD(TAG, "waiting for brake relay to close (IDLE)...");
				gpio_set_level(config.gpio_a, config.aEnabledPinState);
				gpio_set_level(config.gpio_b, config.bEnabledPinState);
			}
			break;

		case motorstate_t::FWD:
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
			//forward:
			gpio_set_level(config.gpio_a, config.aEnabledPinState);
			gpio_set_level(config.gpio_b, !config.bEnabledPinState);
			break;

		case motorstate_t::REV:
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel, dutyScaled);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, config.ledc_channel);
			//reverse:
			gpio_set_level(config.gpio_a, !config.aEnabledPinState);
			gpio_set_level(config.gpio_b, config.bEnabledPinState);
			break;
	}
}






//==========================================
//===== sabertooth 2x60A motor driver ======
//==========================================

//-----------------------------
//-------- constructor --------
//-----------------------------
sabertooth2x60a::sabertooth2x60a(sabertooth2x60_config_t config_f){
	config = config_f;
	init();
}


//----------------------------
//----------- init -----------
//----------------------------
void sabertooth2x60a::init(){
	ESP_LOGW(TAG, "initializing uart...");
	uart_config_t uart_config = {
		.baud_rate = 9600, //dip switches: 101011
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	ESP_LOGI(TAG, "configure...");
	ESP_ERROR_CHECK(uart_param_config(config.uart_num, &uart_config));
	ESP_LOGI(TAG, "setpins...");
	ESP_ERROR_CHECK(uart_set_pin(config.uart_num, config.gpio_TX, GPIO_NUM_NC, 0, 0));
	ESP_LOGI(TAG, "init...");
	ESP_ERROR_CHECK(uart_driver_install(config.uart_num, 1024, 1024, 10, NULL, 0)); //todo ring buffer less / 0
	uart_isInitialized = true;
}



//---------- motorCommand to signedDuty ----------
float motorCommandToSignedDuty(motorCommand_t cmd){
	//if (cmd.duty > 100) cmd.duty = 100;
	//else if (cmd.duty < 0) cmd.duty = 0;
	float duty = 0;
	switch (cmd.state){
		case motorstate_t::FWD:
			duty = cmd.duty;
			break;
		case motorstate_t::REV:
			duty = - cmd.duty;
			break;
		case motorstate_t::IDLE:
		case motorstate_t::BRAKE:
			duty = 0;
			break;
	}
	return duty;
}



//-------- send byte --------
//send byte via uart to sabertooth driver
void sabertooth2x60a::sendByte(char data){
	if (!uart_isInitialized){
		ESP_LOGE(TAG, "uart not initialized, not sending command %d...", data);
		return;
	}
	uart_write_bytes(config.uart_num, &data, 1);
	ESP_LOGI(TAG, "sent data=%d to sabertooth driver via uart", data);
}



//---------------------------
//--------- setLeft ---------
//---------------------------
//between 1 and 127 will control motor 1. 1 is full reverse, 64 is stop and 127 is full forward.
//Character 0 (hex 0x00) shut down both motors.
void sabertooth2x60a::setLeft(float dutyPerSigned){
	uint8_t data = 0;
	if (dutyPerSigned <= -100.0) {
		//full reverse for motor 1
		data = 1;
	} else if (dutyPerSigned >= 100) {
		//full forward
		data = 127;
	} else if (dutyPerSigned == 0.0) {
		// Stop motor 1
		data = 64;
	} else if (dutyPerSigned < 0.0) {
		//scale negative values between -100 and 0 to  1-63
		data = static_cast<int>(64 - (- dutyPerSigned / 100.0) * 63);
	} else if (dutyPerSigned <= 100.0) {
		//scale positive values between 0 and 100 to control motor 1
		data = static_cast<int>(64 + (dutyPerSigned / 100.0) * 63);
	}
	ESP_LOGI(TAG, "set left motor to duty=%.2f, (data=%d)", dutyPerSigned, data);
	sendByte(data);
}



//----------------------------
//--------- setRight ---------
//----------------------------
//between 128 and 255 will control motor 2. 128 is full reverse, 192 is stop and 255 is full forward.
//Character 0 (hex 0x00) shut down both motors.
void sabertooth2x60a::setRight(float dutyPerSigned) {
	uint8_t data = 0;
	if (dutyPerSigned <= -100.0) {
		// Full reverse for motor 2
		data = 128;
	} else if (dutyPerSigned >= 100.0) {
		// Full forward for motor 2
		data = 255;
	} else if (dutyPerSigned == 0.0) {
		// Stop motor 2
		data = 192; // Assuming 192 represents the stop value for motor 2
	} else if (dutyPerSigned < 0.0) {
		// Scale negative values between -100 and 0 to control motor 2
		data = static_cast<uint8_t>(192 - (-dutyPerSigned / 100.0) * 64);
	} else if (dutyPerSigned <= 100.0) {
		// Scale positive values between 0 and 100 to control motor 2
		data = static_cast<uint8_t>(192 + (dutyPerSigned / 100.0) * 63);
	}

	ESP_LOGW(TAG, "set right motor to duty=%.2f, (data=%d)", dutyPerSigned, data);
	sendByte(data);
}



//-------------------------
//--- setLeft, setRight ---
//-------------------------
//using motorCommand_t struct
void sabertooth2x60a::setLeft(motorCommand_t motorCommand_f){
	float dutyTarget = motorCommandToSignedDuty(motorCommand_f);
	setLeft(dutyTarget);
}
void sabertooth2x60a::setRight(motorCommand_t motorCommand_f){
	float dutyTarget = motorCommandToSignedDuty(motorCommand_f);
	setRight(dutyTarget);
}
