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

#include "driver/uart.h"


//custom C files
//#include "wifi.h"
}

//custom C++ files
//#include "config.hpp"
//#include "control.hpp" 
//#include "button.hpp"
//#include "http.hpp"

//tag for logging
static const char * TAG = "main";





//  //======================================
//  //============ buzzer task =============
//  //======================================
//  //TODO: move the task creation to buzzer class (buzzer.cpp)
//  //e.g. only have function buzzer.createTask() in app_main
//  void task_buzzer( void * pvParameters ){
//      ESP_LOGI("task_buzzer", "Start of buzzer task...");
//          //run function that waits for a beep events to arrive in the queue
//          //and processes them
//          buzzer.processQueue();
//  }
//  
//  
//  
//  //=======================================
//  //============ control task =============
//  //=======================================
//  //task that controls the armchair modes and initiates commands generation and applies them to driver
//  void task_control( void * pvParameters ){
//      ESP_LOGI(TAG, "Initializing controlledArmchair and starting handle loop");
//      //start handle loop (control object declared in config.hpp)
//      control.startHandleLoop();
//  }
//  
//  
//  
//  //======================================
//  //============ button task =============
//  //======================================
//  //task that handles the button interface/commands
//  void task_button( void * pvParameters ){
//      ESP_LOGI(TAG, "Initializing command-button and starting handle loop");
//      //create button instance
//      buttonCommands commandButton(&buttonJoystick, &joystick, &control, &buzzer, &motorLeft, &motorRight);
//      //start handle loop
//      commandButton.startHandleLoop();
//  }
//  
//  
//  
//  //=======================================
//  //============== fan task ===============
//  //=======================================
//  //task that controlls fans for cooling the drivers
//  void task_fans( void * pvParameters ){
//      ESP_LOGI(TAG, "Initializing fans and starting fan handle loop");
//      //create fan instances with config defined in config.cpp
//      controlledFan fan(configCooling, &motorLeft, &motorRight);
//      //repeatedly run fan handle function in a slow loop
//      while(1){
//          fan.handle();
//          vTaskDelay(500 / portTICK_PERIOD_MS);
//      }
//  }
//  
//  
//  
//  //=================================
//  //========== init spiffs ==========
//  //=================================
//  //initialize spi flash filesystem (used for webserver)
//  void init_spiffs(){
//      ESP_LOGI(TAG, "init spiffs");
//      esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
//          .base_path = "/spiffs",
//          .partition_label = NULL,
//          .max_files = 5,
//          .format_if_mount_failed = true};
//      esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);
//  
//      size_t total = 0;
//      size_t used = 0;
//      esp_spiffs_info(NULL, &total, &used);
//  
//      ESP_LOGI(TAG, "SPIFFS: total %d, used %d", total, used);
//      esp_vfs_spiffs_unregister(NULL);
//  }
//  
//  
//  
//  //==================================
//  //======== define loglevels ========
//  //==================================
//  void setLoglevels(void){
//      //set loglevel for all tags:
//      esp_log_level_set("*", ESP_LOG_WARN);
//  
//      //--- set loglevel for individual tags ---
//      esp_log_level_set("main", ESP_LOG_INFO);
//      esp_log_level_set("buzzer", ESP_LOG_ERROR);
//      //esp_log_level_set("motordriver", ESP_LOG_INFO);
//      //esp_log_level_set("motor-control", ESP_LOG_DEBUG);
//      //esp_log_level_set("evaluatedJoystick", ESP_LOG_DEBUG);
//      //esp_log_level_set("joystickCommands", ESP_LOG_DEBUG);
//      esp_log_level_set("button", ESP_LOG_INFO);
//      esp_log_level_set("control", ESP_LOG_INFO);
//      esp_log_level_set("fan-control", ESP_LOG_INFO);
//      esp_log_level_set("wifi", ESP_LOG_INFO);
//      esp_log_level_set("http", ESP_LOG_INFO);
//      esp_log_level_set("automatedArmchair", ESP_LOG_DEBUG);
//      //esp_log_level_set("current-sensors", ESP_LOG_INFO);
//  }




static void uart_task(void *arg)
{
    uart_config_t uart1_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

	ESP_LOGW(TAG, "config...");
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart1_config));
	ESP_LOGW(TAG, "setpins...");
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, 23, 22, 0, 0));
	ESP_LOGW(TAG, "init...");
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 1024, 10, NULL, 0));
    
    uint8_t *data = (uint8_t *) malloc(1024);

	//SEND data to motorctl board
	uint8_t count = 0;
	ESP_LOGW(TAG, "startloop...");
    while (1) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
        int len = uart_read_bytes(UART_NUM_1, data, (1024 - 1), 20 / portTICK_PERIOD_MS);
		uart_flush_input(UART_NUM_1);
		uart_flush(UART_NUM_1);
		ESP_LOGW(TAG, "received data %d", *data);
		*data = 99;
        uart_write_bytes(UART_NUM_1, (const char *) &count, 1);
		ESP_LOGW(TAG, "sent data %d", count);
		count++;
    }
}



//=================================
//=========== app_main ============
//=================================
extern "C" void app_main(void) {
//    //enable 5V volate regulator
//    gpio_pad_select_gpio(GPIO_NUM_17);                                                  
//    gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);
//    gpio_set_level(GPIO_NUM_17, 1);                                                      
//
//    //---- define log levels ----
//	setLoglevels();
//    
//    //------------------------------
//    //--- create task for buzzer ---
//    //------------------------------
//    xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 2, NULL);
//
//    //-------------------------------
//    //--- create task for control ---
//    //-------------------------------
//    //task that generates motor commands depending on the current mode and sends those to motorctl task
//    xTaskCreate(&task_control, "task_control", 4096, NULL, 5, NULL);
//
//    //------------------------------
//    //--- create task for button ---
//    //------------------------------
//    //task that evaluates and processes the button input and runs the configured commands
//    xTaskCreate(&task_button, "task_button", 4096, NULL, 4, NULL);
//
//    //-----------------------------------
//    //--- create task for fan control ---
//    //-----------------------------------
//    //task that evaluates and processes the button input and runs the configured commands
//    xTaskCreate(&task_fans, "task_fans", 2048, NULL, 1, NULL);
//
//
//    //beep at startup
//    buzzer.beep(3, 70, 50);
//
//    //--- initialize nvs-flash and netif (needed for wifi) ---
//    wifi_initNvs_initNetif();
//
//    //--- initialize spiffs ---
//    init_spiffs();
//
//    //--- initialize and start wifi ---
//    //FIXME: run wifi_init_client or wifi_init_ap as intended from control.cpp when switching state 
//    //currently commented out because of error "assert failed: xQueueSemaphoreTake queue.c:1549 (pxQueue->uxItemSize == 0)" when calling control->changeMode from button.cpp
//    //when calling control.changeMode(http) from main.cpp it worked without error for some reason?
//    ESP_LOGI(TAG,"starting wifi...");
//    //wifi_init_client(); //connect to existing wifi
//    wifi_init_ap(); //start access point
//    ESP_LOGI(TAG,"done starting wifi");
//
//
//    //--- testing http server ---
//    //    wifi_init_client(); //connect to existing wifi
//    //    vTaskDelay(2000 / portTICK_PERIOD_MS);
//    //    ESP_LOGI(TAG, "initializing http server");
//    //    http_init_server();
//    
//
//    //--- testing force http mode after startup ---
//        //control.changeMode(controlMode_t::HTTP);
//
//
//	//--- main loop ---
//	//does nothing except for testing things


	//TESTING UART
	    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);

	while(1){
		vTaskDelay(1000 / portTICK_PERIOD_MS);

		//---------------------------------
		//-------- TESTING section --------
		//---------------------------------

	}

}
