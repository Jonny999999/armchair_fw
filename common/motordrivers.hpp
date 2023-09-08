#pragma once

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/uart.h"
}

#include <cmath>


//====================================
//===== single100a motor driver ======
//====================================

//--------------------------------------------
//---- struct, enum, variable declarations ---
//--------------------------------------------
//motorstate_t, motorstateStr outsourced to common/types.hpp
#include "types.hpp"

//struct with all config parameters for single100a motor driver
typedef struct single100a_config_t {
	gpio_num_t gpio_pwm;
	gpio_num_t gpio_a;
	gpio_num_t gpio_b;
	gpio_num_t gpio_brakeRelay;
	ledc_timer_t ledc_timer;
	ledc_channel_t ledc_channel;
	bool aEnabledPinState;
	bool bEnabledPinState;
	ledc_timer_bit_t resolution;
	int pwmFreq;
} single100a_config_t;



//--------------------------------
//------- single100a class -------
//--------------------------------
class single100a {
	public:
		//--- constructor ---
		single100a(single100a_config_t config_f); //provide config struct (see above)

		//--- functions ---
		void set(motorstate_t state, float duty_f = 0); //set mode and duty of the motor (see motorstate_t above)
		void set(motorCommand_t cmd);
		//TODO: add functions to get the current state and duty

	private:
		//--- functions ---
		void init(); //initialize pwm and gpio outputs, calculate maxDuty

		//--- variables ---
		single100a_config_t config;
		uint32_t dutyMax;
		motorstate_t state = motorstate_t::IDLE;
		bool brakeWaitingForRelay = false;
		uint32_t timestamp_brakeRelayPowered;
};











//==========================================
//===== sabertooth 2x60A motor driver ======
//==========================================

//struct with all config parameters for sabertooth2x60a driver
typedef struct  {
	gpio_num_t gpio_TX;
	uart_port_t uart_num; //(UART_NUM_1/2)
} sabertooth2x60_config_t;


//controll via simplified serial
//=> set dip switches to 'simplified serial 9600 Baud' according to manual 101011
class sabertooth2x60a {
	public:
		//--- constructor ---
		sabertooth2x60a(sabertooth2x60_config_t config_f); //provide config struct (see above)
		//--- functions ---
		//set motor speed with float value from -100 to 100
		void setLeft(float dutyPerSigned);
		void setRight(float dutyPerSigned);
		//set mode and duty of the motor (see motorstate_t above)
		void setLeft(motorCommand_t command); 
		void setRight(motorCommand_t command);
		//TODO: add functions to get the current state and duty

	private:
		//--- functions ---
		void sendByte(char data);
		void init(); //initialize uart

		//--- variables ---
		sabertooth2x60_config_t config;
		uint32_t dutyMax;
		motorstate_t state = motorstate_t::IDLE;
		bool uart_isInitialized = false;
};




//   //wrap motor drivers in a common class
//   //-> different drivers can be used easily without changing code in motorctl etc
//   //TODO add UART as driver?
//   enum driverType_t {SINGLE100A, SABERTOOTH};
//   
//   template <typename driverType> class motors_t {
//   	public:
//   		motors_t (single100a_config_t config_left, single100a_config_t config_right);
//   		motors_t (sabertooth2x60_config_t);
//   
//   		setLeft(motorstate_t state);
//   		setRight(motorstate_t state);
//   		set(motorCommands_t);
//   	private:
//   		enum driverType_t driverType;
//   		sabertooth2x60_config_t config_saber;
//   		single100a_config_t config_single;
//   }
//   
//   

