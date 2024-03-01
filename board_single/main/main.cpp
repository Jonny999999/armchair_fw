extern "C"
{
#include <stdio.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_spiffs.h"    

//custom C files
#include "wifi.h"
}

#include <new>

//custom C++ files
//folder common
#include "uart_common.hpp"
#include "motordrivers.hpp"
#include "http.hpp"
#include "speedsensor.hpp"
#include "motorctl.hpp"

//folder single_board
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

// TODO initialize driver in createOjects like everything else
// (as in 6e9b3d96d96947c53188be1dec421bd7ff87478e) 
// issue with laggy encoder wenn calling methods via pointer though
//sabertooth2x60a *sabertoothDriver;
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
	//TODO why encoder lag when call via pointer?
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

//--- tag for logging ---
static const char * TAG = "main";

//-- handle passed to tasks for accessing nvs --
nvs_handle_t nvsHandle;




//=================================
//========== init spiffs ==========
//=================================
//initialize spi flash filesystem (used for webserver)
void init_spiffs(){
    ESP_LOGW(TAG, "initializing spiffs...");
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
    // create sabertooth motor driver instance
    // sabertooth2x60a sabertoothDriver(sabertoothConfig);
    // with configuration above
	//sabertoothDriver = new sabertooth2x60a(sabertoothConfig);

	// create controlled motor instances (motorctl.hpp)
    // with configurations from config.cpp
    motorLeft = new controlledMotor(setLeftFunc, configMotorControlLeft, &nvsHandle);
    motorRight = new controlledMotor(setRightFunc, configMotorControlRight, &nvsHandle);

    // create speedsensor instances
    // with configurations from config.cpp
    speedLeft = new speedSensor(speedLeft_config);
    speedRight = new speedSensor(speedRight_config);

    // create joystick instance (joystick.hpp)
    joystick = new evaluatedJoystick(configJoystick, &nvsHandle);

    // create httpJoystick object (http.hpp)
    httpJoystickMain = new httpJoystick(configHttpJoystickMain);
    http_init_server(on_joystick_url);

    // create buzzer object on pin 12 with gap between queued events of 1ms
    buzzer = new buzzer_t(GPIO_NUM_12, 1);

    // create objects for controlling the chair position
    //                       gpio_up, gpio_down, name
    legRest = new cControlledRest(GPIO_NUM_2, GPIO_NUM_15, "legRest");
    backRest = new cControlledRest(GPIO_NUM_16, GPIO_NUM_4, "backRest");

    // create control object (control.hpp)
    // with configuration from config.cpp
    control = new controlledArmchair(configControl, buzzer, motorLeft, motorRight, joystick, &joystickGenerateCommands_config, httpJoystickMain, automatedArmchair, legRest, backRest, &nvsHandle);

    // create automatedArmchair_c object (for auto-mode) (auto.hpp)
    automatedArmchair = new automatedArmchair_c(motorLeft, motorRight);

}




//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {
	ESP_LOGW(TAG, "===== BOOT (pre main) Completed =====\n");

	ESP_LOGW(TAG, "===== INITIALIZING COMPONENTS =====");
	//--- define log levels ---
	setLoglevels();

	//--- enable 5V volate regulator ---
	ESP_LOGW(TAG, "enabling 5V regulator...");
	gpio_pad_select_gpio(GPIO_NUM_17);                                                  
	gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_17, 1);                                                      

	//--- initialize nvs-flash and netif (needed for wifi) ---
	ESP_LOGW(TAG,"initializing wifi...");
	wifi_initNvs_initNetif();

	//--- initialize spiffs ---
	init_spiffs();

	//--- initialize and start wifi ---
	ESP_LOGW(TAG,"starting wifi...");
	//wifi_init_client(); //connect to existing wifi
	wifi_init_ap(); //start access point
	ESP_LOGD(TAG,"done starting wifi");

	//--- initialize encoder ---
	const QueueHandle_t encoderQueue = encoder_init(&encoder_config);

	//--- initialize nvs-flash ---  (for persistant config values)
	ESP_LOGW(TAG, "initializing nvs-flash...");
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_LOGE(TAG, "NVS truncated -> deleting flash");
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
	//--- open nvs-flash ---
	err = nvs_open("storage", NVS_READWRITE, &nvsHandle);
	if (err != ESP_OK)
		ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));

	printf("\n");



	//--- create all objects ---
	ESP_LOGW(TAG, "===== CREATING SHARED OBJECTS =====");

	//initialize sabertooth object in STACK (due to performance issues in heap)
	///sabertoothDriver = static_cast<sabertooth2x60a*>(alloca(sizeof(sabertooth2x60a)));
	///new (sabertoothDriver) sabertooth2x60a(sabertoothConfig);

	//create all class instances used below in HEAP
	createObjects();

	printf("\n");



	//--- create tasks ---
	ESP_LOGW(TAG, "===== CREATING TASKS =====");

	//----------------------------------------------
	//--- create task for controlling the motors ---
	//----------------------------------------------
	//task for each motor that handles to following:
	//receives commands from control via queue, handle ramp and current, apply new duty by passing it to method of motordriver (ptr)
	xTaskCreate(&task_motorctl, "task_ctl-left-motor", 2*4096, motorLeft, 6, NULL);
	xTaskCreate(&task_motorctl, "task_ctl-right-motor", 2*4096, motorRight, 6, NULL);

	//------------------------------
	//--- create task for buzzer ---
	//------------------------------
	//task that processes queued beeps
	//note: pointer to shard object 'buzzer' is passed as task parameter:
	xTaskCreate(&task_buzzer, "task_buzzer", 2048, buzzer, 2, NULL);

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
	xTaskCreate(&task_button, "task_button", 4096, &button_param, 3, NULL);

	//-----------------------------------
	//--- create task for fan control ---
	//-----------------------------------
	//task that controls cooling fans of the motor driver
	task_fans_parameters_t fans_param = {configFans, motorLeft, motorRight};
	xTaskCreate(&task_fans, "task_fans", 2048, &fans_param, 1, NULL);

	//-----------------------------------
	//----- create task for display -----
	//-----------------------------------
	//task that handles the display (show stats, handle menu in 'MENU' mode)
	display_task_parameters_t display_param = {display_config, control, joystick, encoderQueue, motorLeft, motorRight, speedLeft, speedRight, buzzer, &nvsHandle};
	xTaskCreate(&display_task, "display_task", 3*2048, &display_param, 3, NULL);

	vTaskDelay(200 / portTICK_PERIOD_MS); //wait for all tasks to finish initializing
	printf("\n");



	//--- startup finished ---
	ESP_LOGW(TAG, "===== STARTUP FINISHED =====\n");
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
		vTaskDelay(portMAX_DELAY);
		//vTaskDelay(5000 / portTICK_PERIOD_MS);

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
