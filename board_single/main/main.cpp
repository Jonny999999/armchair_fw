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

//custom C files
#include "wifi.h"
}

//custom C++ files
//folder common
#include "uart_common.hpp"
#include "motordrivers.hpp"
#include "http.hpp"
#include "types.hpp"
#include "speedsensor.hpp"
#include "motorctl.hpp"

//folder single_board
#include "config.hpp"
#include "control.hpp" 
#include "button.hpp"
#include "display.hpp"
#include "encoder.hpp"


//only extends this file (no library):
//outsourced all configuration related structures
#include "config.cpp"



//================================
//======== declarations ==========
//================================
//--- declare all pointers to shared objects ---
controlledMotor *motorLeft;
controlledMotor *motorRight;

//create sabertooth motor driver instance (in stack, direct usw, without pointer)
sabertooth2x60a sabertoothDriver(sabertoothConfig);

evaluatedJoystick *joystick;

buzzer_t *buzzer;

controlledArmchair *control;

automatedArmchair_c *automatedArmchair;

httpJoystick *httpJoystickMain;

speedSensor *speedLeft;
speedSensor *speedRight;

cControlledRest *legRest;
cControlledRest *backRest;


//--- lambda functions motor-driver ---
// functions for updating the duty via currently used motor driver (hardware) that can then be passed to controlledMotor
//-> makes it possible to easily use different motor drivers
motorSetCommandFunc_t setLeftFunc = [&sabertoothDriver](motorCommand_t cmd)
{
    sabertoothDriver.setLeft(cmd);
};
motorSetCommandFunc_t setRightFunc = [&sabertoothDriver](motorCommand_t cmd)
{
    sabertoothDriver.setRight(cmd);
};

//--- lambda function http-joystick ---
// function that initializes the http server requires a function pointer to function that handels each url
// the httpd_uri config struct does not accept a pointer to a method of a class instance, directly
// thus this lambda function is necessary:
// declare pointer to receiveHttpData method of httpJoystick class
esp_err_t (httpJoystick::*pointerToReceiveFunc)(httpd_req_t *req) = &httpJoystick::receiveHttpData;
esp_err_t on_joystick_url(httpd_req_t *req)
{
    // run pointer to receiveHttpData function of httpJoystickMain instance
    return (httpJoystickMain->*pointerToReceiveFunc)(req);
}

//tag for logging
static const char * TAG = "main";



//======================================
//============ buzzer task =============
//======================================
//TODO: move the task creation to buzzer class (buzzer.cpp)
//e.g. only have function buzzer.createTask() in app_main
void task_buzzer( void * pvParameters ){
    ESP_LOGI("task_buzzer", "Start of buzzer task...");
        //run function that waits for a beep events to arrive in the queue
        //and processes them
        buzzer->processQueue();
}



//=======================================
//============== fan task ===============
//=======================================
//TODO: move this definition to fan.cpp
//task that controlls fans for cooling the drivers
void task_fans( void * pvParameters ){
    ESP_LOGI(TAG, "Initializing fans and starting fan handle loop");
    //create fan instances with config defined in config.cpp
    controlledFan fan(configCooling, motorLeft, motorRight);
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





//=================================
//========= createObjects =========
//=================================
//create all shared objects
//their references can be passed to the tasks that need access in main

//Note: the configuration structures (e.g. configMotorControlLeft) are outsourced to file 'config.cpp'

void createObjects()
{
    // create controlled motor instances (motorctl.hpp)
    // with configurations above
    motorLeft = new controlledMotor(setLeftFunc, configMotorControlLeft);
    motorRight = new controlledMotor(setRightFunc, configMotorControlRight);

    // create speedsensor instances
    // with configurations above
    speedLeft = new speedSensor(speedLeft_config);
    speedRight = new speedSensor(speedRight_config);

    // create joystic instance (joystick.hpp)
    joystick = new evaluatedJoystick(configJoystick);

    // create httpJoystick object (http.hpp)
    httpJoystickMain = new httpJoystick(configHttpJoystickMain);
    http_init_server(on_joystick_url);

    // create buzzer object on pin 12 with gap between queued events of 100ms
    buzzer = new buzzer_t(GPIO_NUM_12, 100);

    // create control object (control.hpp)
    // with configuration above
    control = new controlledArmchair(configControl, buzzer, motorLeft, motorRight, joystick, httpJoystickMain, automatedArmchair, legRest, backRest);

    // create automatedArmchair_c object (for auto-mode) (auto.hpp)
    automatedArmchair = new automatedArmchair_c(motorLeft, motorRight);

    // create objects for controlling the chair position
    //                       gpio_up, gpio_down, name
    legRest = new cControlledRest(GPIO_NUM_4, GPIO_NUM_16, "legRest");
    backRest = new cControlledRest(GPIO_NUM_2, GPIO_NUM_15, "backRest");
}




//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {

	ESP_LOGW(TAG, "===== INITIALIZING COMPONENTS =====");
	//--- define log levels ---
	setLoglevels();

	//--- enable 5V volate regulator ---
	ESP_LOGW(TAG, "enabling 5V regulator...");
	gpio_pad_select_gpio(GPIO_NUM_17);                                                  
	gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_17, 1);                                                      

	//--- initialize nvs-flash and netif (needed for wifi) ---
	wifi_initNvs_initNetif();

	//--- initialize spiffs ---
	init_spiffs();

	//--- initialize and start wifi ---
	ESP_LOGD(TAG,"starting wifi...");
	//wifi_init_client(); //connect to existing wifi
	wifi_init_ap(); //start access point
	ESP_LOGD(TAG,"done starting wifi");

	//--- initialize encoder ---
	const QueueHandle_t encoderQueue = encoder_init();



	//--- create all objects ---
	ESP_LOGW(TAG, "===== CREATING SHARED OBJECTS =====");

	//create all class instances used below
	//see 'createObjects.hpp'
	createObjects();



#ifndef ENCODER_TEST
	//--- create tasks ---
	ESP_LOGW(TAG, "===== CREATING TASKS =====");

	//----------------------------------------------
	//--- create task for controlling the motors ---
	//----------------------------------------------
	//task that receives commands, handles ramp and current limit and executes commands using the motordriver function
	task_motorctl_parameters_t motorctl_param = {motorLeft, motorRight};
	xTaskCreate(&task_motorctl, "task_motor-control", 2*4096, &motorctl_param, 6, NULL);

	//------------------------------
	//--- create task for buzzer ---
	//------------------------------
	xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 2, NULL);

	//-------------------------------
	//--- create task for control ---
	//-------------------------------
	//task that generates motor commands depending on the current mode and sends those to motorctl task
	//note: pointer to shared object 'control' is passed as task parameter:
	xTaskCreate(&task_control, "task_control", 4096, control, 5, NULL);

	//------------------------------
	//--- create task for button ---
	//------------------------------
	//task that handles button/encoder events in any mode except 'MENU' (e.g. switch modes by pressing certain count)
	task_button_parameters_t button_param = {control, joystick, encoderQueue, motorLeft, motorRight, buzzer};
	xTaskCreate(&task_button, "task_button", 4096, &button_param, 4, NULL);

	//-----------------------------------
	//--- create task for fan control ---
	//-----------------------------------
	//task that controls cooling fans of the motor driver
	xTaskCreate(&task_fans, "task_fans", 2048, NULL, 1, NULL);

	//-----------------------------------
	//----- create task for display -----
	//-----------------------------------
	////task that handles the display (show stats, handle menu in 'MENU' mode)
	display_task_parameters_t display_param = {control, joystick, encoderQueue, motorLeft, motorRight, speedLeft, speedRight, buzzer};
	xTaskCreate(&display_task, "display_task", 3*2048, &display_param, 1, NULL);


#endif

	//--- startup finished ---
	ESP_LOGW(TAG, "===== STARTUP FINISHED =====");
	buzzer->beep(3, 70, 50);

	//--- testing encoder ---
	//xTaskCreate(&task_encoderExample, "task_buzzer", 2048, encoderQueue, 2, NULL);

	//--- testing http server ---
	//    wifi_init_client(); //connect to existing wifi
	//    vTaskDelay(2000 / portTICK_PERIOD_MS);
	//    ESP_LOGI(TAG, "initializing http server");
	//    http_init_server();


	//--- testing force specific mode after startup ---
	//control->changeMode(controlMode_t::MENU);



	//--- main loop ---
	//does nothing except for testing things
	while(1){
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		//---------------------------------
		//-------- TESTING section --------
		//---------------------------------
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
