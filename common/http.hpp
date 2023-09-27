#pragma once

extern "C"
{
#include "esp_http_server.h"
}

#include "joystick.hpp"



//============================
//===== init http server =====
//============================
//function that initializes http server and configures available urls
void http_init_server();


//==============================
//===== start mdns service =====
//==============================
//function that initializes and starts mdns server for host discovery
void start_mdns_service();


//============================
//===== stop http server =====
//============================
//function that destroys the http server
void http_stop_server();


//==============================
//===== httpJoystick class =====
//==============================
//class that receices that from a HTTP post request, generates and scales joystick data and provides the data in a queue

//struct with configuration parameters
typedef struct httpJoystick_config_t {
    float toleranceZeroX_Per;//percentage around joystick axis the coordinate snaps to 0
    float toleranceZeroY_Per;
    float toleranceEndPer; //percentage before joystick end the coordinate snaps to 1/-1
    uint32_t timeoutMs;    //time no new data was received before the motors get turned off
} httpJoystick_config_t;


class httpJoystick{
    public:
        //--- constructor ---
        httpJoystick( httpJoystick_config_t config_f );

        //--- functions ---
        joystickData_t getData(); //wait for and return joystick data from queue, if timeout return CENTER

        esp_err_t receiveHttpData(httpd_req_t *req);  //function that is called when data is received with post request at /api/joystick

    private:
        //--- variables ---
        httpJoystick_config_t config;
        QueueHandle_t joystickDataQueue = xQueueCreate( 1, sizeof( struct joystickData_t ) );
        //struct for receiving data from http function, and storing data of last update
        joystickData_t dataRead;
        const joystickData_t dataCenter = {
            .position = joystickPos_t::CENTER,
            .x = 0,
            .y = 0,
            .radius = 0,
            .angle = 0
        };
};



//===== global object =====
//create global instance of httpJoystick
//note: is constructed/configured in config.cpp
extern httpJoystick httpJoystickMain;
