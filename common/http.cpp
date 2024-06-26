extern "C"
{
#include <stdio.h>
#include "mdns.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"

}

#include "http.hpp"
//#include "config.hpp"


//tag for logging
static const char * TAG = "http";
static httpd_handle_t server = NULL;



//==============================
//===== start mdns service =====
//==============================
//TODO: test this, not working?
//function that initializes and starts mdns server for host discovery
void start_mdns_service()
{
//init queue for sending joystickdata from http endpoint to control task
  mdns_init();
  mdns_hostname_set("armchair");
  mdns_instance_name_set("electric armchair");
}



//===========================
//======= default url =======
//===========================
//serve requested files from spiffs
static esp_err_t on_default_url(httpd_req_t *req)
{
  ESP_LOGI(TAG, "Opening page for URL: %s", req->uri);

  esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true};
  esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);

  char path[600];
  if (strcmp(req->uri, "/") == 0)
    strcpy(path, "/spiffs/index.html");
  else
    sprintf(path, "/spiffs%s", req->uri);
  char *ext = strrchr(path, '.');
  if (ext == NULL || strncmp(ext, ".local", strlen(".local")) == 0)
  {
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
  }
  if (strcmp(ext, ".css") == 0)
    httpd_resp_set_type(req, "text/css");
  if (strcmp(ext, ".js") == 0)
    httpd_resp_set_type(req, "text/javascript");
  if (strcmp(ext, ".png") == 0)
    httpd_resp_set_type(req, "image/png");

  FILE *file = fopen(path, "r");
  if (file == NULL)
  {
    httpd_resp_send_404(req);
    esp_vfs_spiffs_unregister(NULL);
    return ESP_OK;
  }

  char lineRead[256];
  while (fgets(lineRead, sizeof(lineRead), file))
  {
    httpd_resp_sendstr_chunk(req, lineRead);
  }
  httpd_resp_sendstr_chunk(req, NULL);

  esp_vfs_spiffs_unregister(NULL);
  return ESP_OK;
}



//==============================
//===== httpJoystick class =====
//==============================
//-----------------------
//----- constructor -----
//-----------------------
httpJoystick::httpJoystick( httpJoystick_config_t config_f ){
    //copy config struct
    config = config_f;
}


//--------------------------
//---- receiveHttpData -----
//--------------------------
//joystick endpoint - function that is called when data is received with post request at /api/joystick
esp_err_t httpJoystick::receiveHttpData(httpd_req_t *req){ 
    //--- add header ---
    //to allow cross origin (otherwise browser fails when app is running on another host)
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    //--- get data from http request ---
    char buffer[100];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len);
    ESP_LOGD(TAG, "/api/joystick: received data: %s", buffer);

    //--- parse received json string to json object ---
    cJSON *payload = cJSON_Parse(buffer);
    ESP_LOGV(TAG, "parsed json: \n %s", cJSON_Print(payload));

    //--- extract relevant items from json object ---
    cJSON *x_json = cJSON_GetObjectItem(payload, "x");  
    cJSON *y_json = cJSON_GetObjectItem(payload, "y");  

    //--- save items to struct ---
    joystickData_t data = { };

    //note cjson can only interpret values as numbers when there are no quotes around the values in json (are removed from json on client side)
    //convert json to double to float
    data.x = static_cast<float>(x_json->valuedouble);
    data.y = static_cast<float>(y_json->valuedouble);
    //log received and parsed values
    ESP_LOGI(TAG, "received values: x=%.3f  y=%.3f",
            data.x, data.y);

    // scaleCoordinate(input, min, max, center, tolerance_zero_per, tolerance_end_per)
    data.x = scaleCoordinate(data.x+1, 0, 2, 1, config.toleranceZeroX_Per, config.toleranceEndPer); 
    data.y = scaleCoordinate(data.y+1, 0, 2, 1, config.toleranceZeroY_Per, config.toleranceEndPer);

    //--- calculate radius with new/scaled coordinates ---
    data.radius = sqrt(pow(data.x,2) + pow(data.y,2));
    //TODO: radius tolerance? (as in original joystick func)
    //limit radius to 1
    if (data.radius > 1) {
        data.radius = 1;
    }
    //--- calculate angle ---
    data.angle = (atan(data.y/data.x) * 180) / 3.141;
    //--- evaluate position ---
    data.position = joystick_evaluatePosition(data.x, data.y);

    //log processed values
    ESP_LOGI(TAG, "processed values: x=%.3f  y=%.3f  radius=%.3f  angle=%.3f  pos=%s",
            data.x, data.y, data.radius, data.angle, joystickPosStr[(int)data.position]);

    //--- free memory ---
    cJSON_Delete(payload);

    //--- send data to control task via queue ---
    //xQueueSend( joystickDataQueue, ( void * )&data, ( TickType_t ) 0 );
    //changed to length = 1  -> overwrite - older values are no longer relevant
    xQueueOverwrite( joystickDataQueue, ( void * )&data );

    //--- return http response ---
    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}


//-------------------
//----- getData -----
//-------------------
//wait for and return joystick data from queue, return last data if nothing received within 500ms, return center data when timeout exceeded
joystickData_t httpJoystick::getData(){

    //--- get joystick data from queue ---
    if( xQueueReceive( joystickDataQueue, &dataRead, pdMS_TO_TICKS(500) ) ) { //dont wait longer than 500ms to not block the control loop for too long
        ESP_LOGD(TAG, "getData: received data (from queue): x=%.3f  y=%.3f  radius=%.3f  angle=%.3f",
                dataRead.x, dataRead.y, dataRead.radius, dataRead.angle);
        timeLastData = esp_log_timestamp();
    }
    //--- timeout ---
    // send error message when last received data did NOT result in CENTER position and timeout exceeded
    else { 
        if (dataRead.position != joystickPos_t::CENTER && (esp_log_timestamp() - timeLastData) > config.timeoutMs) {
            //change data to "joystick center" data to stop the motors
            dataRead = dataCenter;
            ESP_LOGE(TAG, "TIMEOUT - no data received for %dms -> set to center", config.timeoutMs);
        }
    }
    return dataRead;
}



//============================
//===== init http server =====
//============================
//function that initializes http server and configures available url's

//parameter: provide pointer to function that handle incomming joystick data (for configuring the url)
//TODO add handle functions to future additional endpoints/urls here too
void http_init_server(http_handler_t onJoystickUrl)
{
  ESP_LOGI(TAG, "initializing HTTP-Server...");

  //---- configure webserver ----
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;

  //---- start webserver ----
  ESP_ERROR_CHECK(httpd_start(&server, &config));


  //----- define URLs -----
  //note: dont use separate assignment of elements because causes controller crash
    httpd_uri_t joystick_url = {
      .uri = "/api/joystick",
      .method = HTTP_POST,
      .handler = onJoystickUrl,
      };
  httpd_register_uri_handler(server, &joystick_url);

  httpd_uri_t default_url = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = on_default_url};
  httpd_register_uri_handler(server, &default_url);


  //previous approach with sockets:
    //  httpd_uri_t socket_joystick_url = {
    //      .uri = "/ws-api/joystick",
    //      .method = HTTP_GET,
    //      .handler = on_socket_joystick_url,
    //      .is_websocket = true};
    //  httpd_register_uri_handler(server, &socket_joystick_url);

}



//============================
//===== stop http server =====
//============================
//function that destroys the http server
void http_stop_server()
{
  ESP_LOGW(TAG, "stopping HTTP-Server");
  httpd_stop(server);
}



