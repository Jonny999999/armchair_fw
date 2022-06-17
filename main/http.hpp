#pragma once

extern QueueHandle_t joystickDataQueue;

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
