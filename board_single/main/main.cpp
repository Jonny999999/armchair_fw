#include "hal/uart_types.h"
#include "motordrivers.hpp"
#include "types.hpp"
extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_spiffs.h"    

#include "driver/ledc.h"

//custom C files
#include "wifi.h"
}

//custom C++ files
#include "config.hpp"
#include "control.hpp" 
#include "button.hpp"
#include "http.hpp"

#include "uart_common.hpp"

#include "display.hpp"

//tag for logging
static const char * TAG = "main";



//====================================
//========== motorctl task ===========
//====================================
//task for handling the motors (ramp, current limit, driver)
void task_motorctl( void * pvParameters ){
    ESP_LOGI(TAG, "starting handle loop...");
    while(1){
        motorRight.handle();
        motorLeft.handle();
        //10khz -> T=100us
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}



//======================================
//============ buzzer task =============
//======================================
//TODO: move the task creation to buzzer class (buzzer.cpp)
//e.g. only have function buzzer.createTask() in app_main
void task_buzzer( void * pvParameters ){
    ESP_LOGI("task_buzzer", "Start of buzzer task...");
        //run function that waits for a beep events to arrive in the queue
        //and processes them
        buzzer.processQueue();
}



//=======================================
//============ control task =============
//=======================================
//task that controls the armchair modes and initiates commands generation and applies them to driver
void task_control( void * pvParameters ){
    ESP_LOGI(TAG, "Initializing controlledArmchair and starting handle loop");
    //start handle loop (control object declared in config.hpp)
    control.startHandleLoop();
}



//======================================
//============ button task =============
//======================================
//task that handles the button interface/commands
void task_button( void * pvParameters ){
    ESP_LOGI(TAG, "Initializing command-button and starting handle loop");
    //create button instance
    buttonCommands commandButton(&buttonJoystick, &joystick, &control, &buzzer, &motorLeft, &motorRight);
    //start handle loop
    commandButton.startHandleLoop();
}



//=======================================
//============== fan task ===============
//=======================================
//task that controlls fans for cooling the drivers
void task_fans( void * pvParameters ){
    ESP_LOGI(TAG, "Initializing fans and starting fan handle loop");
    //create fan instances with config defined in config.cpp
    controlledFan fan(configCooling, &motorLeft, &motorRight);
    //repeatedly run fan handle function in a slow loop
    while(1){
        fan.handle();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}



//=================================
//========== init spiffs ==========
//=================================
//initialize spi flash filesystem (used for webserver)
void init_spiffs(){
    ESP_LOGI(TAG, "init spiffs");
    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);

    size_t total = 0;
    size_t used = 0;
    esp_spiffs_info(NULL, &total, &used);

    ESP_LOGI(TAG, "SPIFFS: total %d, used %d", total, used);
    esp_vfs_spiffs_unregister(NULL);
}



//==================================
//======== define loglevels ========
//==================================
void setLoglevels(void){
    //set loglevel for all tags:
    esp_log_level_set("*", ESP_LOG_WARN);

    //--- set loglevel for individual tags ---
    esp_log_level_set("main", ESP_LOG_INFO);
    esp_log_level_set("buzzer", ESP_LOG_ERROR);
    //esp_log_level_set("motordriver", ESP_LOG_ERROR);
    //esp_log_level_set("motor-control", ESP_LOG_INFO);
	esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
    //esp_log_level_set("joystickCommands", ESP_LOG_DEBUG);
    esp_log_level_set("button", ESP_LOG_INFO);
    esp_log_level_set("control", ESP_LOG_INFO);
    esp_log_level_set("fan-control", ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_INFO);
    esp_log_level_set("http", ESP_LOG_INFO);
    esp_log_level_set("automatedArmchair", ESP_LOG_DEBUG);
    //esp_log_level_set("current-sensors", ESP_LOG_INFO);
}


//send byte via uart to test sabertooth driver
void sendByte(char data){
	uart_write_bytes(UART_NUM_1, &data, 1);
	ESP_LOGI(TAG, "sent %x  /  %d via uart", data, data);
}


//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {
	//enable 5V volate regulator
	gpio_pad_select_gpio(GPIO_NUM_17);                                                  
	gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_17, 1);                                                      

	//---- define log levels ----
	setLoglevels();

	//----------------------------------------------
	//--- create task for controlling the motors ---
	//----------------------------------------------
	//task that receives commands, handles ramp and current limit and executes commands using the motordriver function
	xTaskCreate(&task_motorctl, "task_motor-control", 2*4096, NULL, 6, NULL);

	//------------------------------
	//--- create task for buzzer ---
	//------------------------------
	xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 2, NULL);

	//-------------------------------
	//--- create task for control ---
	//-------------------------------
	//task that generates motor commands depending on the current mode and sends those to motorctl task
	xTaskCreate(&task_control, "task_control", 4096, NULL, 5, NULL);

	//------------------------------
	//--- create task for button ---
	//------------------------------
	//task that evaluates and processes the button input and runs the configured commands
	xTaskCreate(&task_button, "task_button", 4096, NULL, 4, NULL);

	//-----------------------------------
	//--- create task for fan control ---
	//-----------------------------------
	//task that evaluates and processes the button input and runs the configured commands
	xTaskCreate(&task_fans, "task_fans", 2048, NULL, 1, NULL);


	//beep at startup
	buzzer.beep(3, 70, 50);

	//--- initialize nvs-flash and netif (needed for wifi) ---
	wifi_initNvs_initNetif();

	//--- initialize spiffs ---
	init_spiffs();

	//--- initialize and start wifi ---
	//FIXME: run wifi_init_client or wifi_init_ap as intended from control.cpp when switching state 
	//currently commented out because of error "assert failed: xQueueSemaphoreTake queue.c:1549 (pxQueue->uxItemSize == 0)" when calling control->changeMode from button.cpp
	//when calling control.changeMode(http) from main.cpp it worked without error for some reason?
	ESP_LOGI(TAG,"starting wifi...");
	//wifi_init_client(); //connect to existing wifi
	wifi_init_ap(); //start access point
	ESP_LOGI(TAG,"done starting wifi");


	//--- testing http server ---
	//    wifi_init_client(); //connect to existing wifi
	//    vTaskDelay(2000 / portTICK_PERIOD_MS);
	//    ESP_LOGI(TAG, "initializing http server");
	//    http_init_server();


	//--- testing force http mode after startup ---
	//control.changeMode(controlMode_t::HTTP);



	//========== display test ============
	startDisplayTest();

	//--- main loop ---
	//does nothing except for testing things
	while(1){
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		//test sabertooth driver
		//		motors.setLeft({motorstate_t::FWD, 70});
		//		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//		motors.setLeft({motorstate_t::IDLE, 0});
		//		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//		motors.setLeft({motorstate_t::REV, 50});
		//		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//		motors.setLeft(-90);
		//		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//		motors.setLeft(90);
		//		vTaskDelay(1000 / portTICK_PERIOD_MS);
		//		motors.setLeft(0);
		//		vTaskDelay(1000 / portTICK_PERIOD_MS);


		//--- test controlledMotor --- (ramp)
		// //brake for 1 s
		//motorLeft.setTarget(motorstate_t::BRAKE);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);
		//command 90% - reverse
		//motorLeft.setTarget(motorstate_t::REV, 90);
		//vTaskDelay(5000 / portTICK_PERIOD_MS);
		//motorLeft.setTarget(motorstate_t::FWD, 80);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);
		//motorLeft.setTarget(motorstate_t::IDLE, 90);
		//vTaskDelay(1000 / portTICK_PERIOD_MS);


		//---------------------------------
		//-------- TESTING section --------
		//---------------------------------
		// //--- test functions at mode change HTTP ---
		// control.changeMode(controlMode_t::HTTP);
		// vTaskDelay(10000 / portTICK_PERIOD_MS);
		// control.changeMode(controlMode_t::IDLE);
		// vTaskDelay(10000 / portTICK_PERIOD_MS);


		//--- test wifi functions ---
		// ESP_LOGI(TAG, "creating AP");
		// wifi_init_ap(); //start accesspoint
		// vTaskDelay(15000 / portTICK_PERIOD_MS);
		// ESP_LOGI(TAG, "stopping wifi");
		// wifi_deinit_ap();  //stop wifi access point
		// vTaskDelay(5000 / portTICK_PERIOD_MS);
		// ESP_LOGI(TAG, "connecting to wifi");
		// wifi_init_client(); //connect to existing wifi
		// vTaskDelay(10000 / portTICK_PERIOD_MS);
		// ESP_LOGI(TAG, "stopping wifi");
		// wifi_deinit_client(); //stop wifi client
		// vTaskDelay(5000 / portTICK_PERIOD_MS);


		//--- test button ---
		//buttonJoystick.handle();
		// if (buttonJoystick.risingEdge){
		//     ESP_LOGI(TAG, "button pressed, was released for %d ms", buttonJoystick.msReleased);
		//     buzzer.beep(2, 100, 50);

		// }else if (buttonJoystick.fallingEdge){
		//     ESP_LOGI(TAG, "button released, was pressed for %d ms", buttonJoystick.msPressed);
		//     buzzer.beep(1, 200, 0);
		// }


		//--- test joystick commands ---
		// motorCommands_t commands = joystick_generateCommandsDriving(joystick);
		// motorRight.setTarget(commands.right.state, commands.right.duty); //TODO make motorctl.setTarget also accept motorcommand struct directly
		// motorLeft.setTarget(commands.left.state, commands.left.duty); //TODO make motorctl.setTarget also accept motorcommand struct directly
		// //motorRight.setTarget(commands.right.state, commands.right.duty);


		//--- test joystick class ---
		//joystickData_t data = joystick.getData();
		//ESP_LOGI(TAG, "position=%s, x=%.1f%%, y=%.1f%%, radius=%.1f%%, angle=%.2f",
		//        joystickPosStr[(int)data.position], data.x*100, data.y*100, data.radius*100, data.angle);

		//--- test the motor driver ---
		//fade up duty - forward
		//   for (int duty=0; duty<=100; duty+=5) {
		//       motorLeft.setTarget(motorstate_t::FWD, duty);
		//       vTaskDelay(100 / portTICK_PERIOD_MS); 
		//   }


	}

}
