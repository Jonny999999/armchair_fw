extern "C"
{
#include <stdio.h>
#include <string.h>
#include <strings.h>
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

extern "C"
{
#include "dns_server.h"
}


//======================================
//===== captive portal / mdns ==========
//======================================
// The armchair AP has no internet access, so phones probe well-known URLs of
// foreign hosts to detect that. Those probes get here because the dns-server
// resolves every hostname to this device. Answering them with a redirect makes
// the phone show the "sign in to network" notification (android/windows) or
// open the captive-portal browser right away (ios), both landing on the web-app
// -> no need to type "192.168.4.1" manually anymore.

//--- config ---
#define AP_IP_STR "192.168.4.1"    // static ip of the soft-AP (see wifi.c)
#define MDNS_HOSTNAME "armchair"   // also reachable as http://armchair.local

//tag for logging
static const char * TAG = "http";
static httpd_handle_t server = NULL;


//==================================
//===== isRequestForOwnHost ========
//==================================
// check whether the client actually wanted to talk to this device
// (everything else is a captive-portal probe due to the hijacked dns)
static bool isRequestForOwnHost(httpd_req_t *req)
{
    char host[64] = "";
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK)
        return true; // no host header at all -> assume direct access

    // strip optional ":port"
    char *colon = strchr(host, ':');
    if (colon)
        *colon = '\0';

    return (strcasecmp(host, AP_IP_STR) == 0
            || strcasecmp(host, MDNS_HOSTNAME) == 0
            || strcasecmp(host, MDNS_HOSTNAME ".local") == 0);
}


//==================================
//===== redirectToWebApp ===========
//==================================
// answer with a redirect to the web-app root (triggers captive portal detection)
static esp_err_t redirectToWebApp(httpd_req_t *req)
{
    ESP_LOGI(TAG, "redirecting request for '%s' to the web-app (captive portal)", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://" AP_IP_STR "/");
    // note: ios only detects a captive portal when the response has content, an empty redirect is not sufficient
    httpd_resp_send(req, "Armchair remote control - redirecting...", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


//=================================
//===== on_404_error ==============
//=================================
// redirect anything the server does not handle to the web-app as well
static esp_err_t on_404_error(httpd_req_t *req, httpd_err_code_t err)
{
    return redirectToWebApp(req);
}


//=================================
//===== setContentType ============
//=================================
// set the http content type according to the file extension
static void setContentType(httpd_req_t *req, const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext == NULL)
        return;

    if (strcmp(ext, ".html") == 0)      httpd_resp_set_type(req, "text/html");
    else if (strcmp(ext, ".css") == 0)  httpd_resp_set_type(req, "text/css");
    else if (strcmp(ext, ".js") == 0)   httpd_resp_set_type(req, "text/javascript");
    else if (strcmp(ext, ".json") == 0) httpd_resp_set_type(req, "application/json");
    else if (strcmp(ext, ".png") == 0)  httpd_resp_set_type(req, "image/png");
    else if (strcmp(ext, ".svg") == 0)  httpd_resp_set_type(req, "image/svg+xml");
    else if (strcmp(ext, ".ico") == 0)  httpd_resp_set_type(req, "image/x-icon");
    else if (strcmp(ext, ".txt") == 0)  httpd_resp_set_type(req, "text/plain");
}


//===========================
//======= default url =======
//===========================
//serve requested files from spiffs
static esp_err_t on_default_url(httpd_req_t *req)
{
    // requests for foreign hosts are captive-portal probes -> redirect to the web-app
    if (!isRequestForOwnHost(req))
        return redirectToWebApp(req);

    ESP_LOGI(TAG, "Opening page for URL: %s", req->uri);

    // strip query string / fragment from the uri to get the file path
    char path[300];
    if (strcmp(req->uri, "/") == 0)
        strcpy(path, "/spiffs/index.html");
    else
    {
        size_t uriLen = strcspn(req->uri, "?#");
        if (uriLen > sizeof(path) - sizeof("/spiffs") - 1)
            uriLen = sizeof(path) - sizeof("/spiffs") - 1;
        snprintf(path, sizeof(path), "/spiffs%.*s", (int)uriLen, req->uri);
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        // unknown path -> send to the web-app (also handles probes that use our own host)
        ESP_LOGW(TAG, "file '%s' not found in spiffs", path);
        return redirectToWebApp(req);
    }

    setContentType(req, path);
    // the hashed bundles in /static/ never change -> let the phone cache them (much faster reconnect)
    if (strncmp(req->uri, "/static/", strlen("/static/")) == 0)
        httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");

    // send the file in chunks
    char buffer[1024];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        if (httpd_resp_send_chunk(req, buffer, read) != ESP_OK)
        {
            ESP_LOGE(TAG, "failed sending '%s' - aborting", path);
            fclose(file);
            httpd_resp_send_chunk(req, NULL, 0); // abort transfer
            return ESP_FAIL;
        }
    }
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
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

  // note: spiffs (webroot) is mounted once at startup in main.cpp and stays mounted
  // (was previously mounted + unmounted again on every single request)

  //---- configure webserver ----
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 6144;      // larger file-buffer is used in on_default_url()
  config.max_uri_handlers = 8;
  config.lru_purge_enable = true; // captive-portal probes open many connections - close the oldest instead of failing

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

  // redirect everything else to the web-app as well (e.g. HEAD requests of captive-portal probes)
  httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, on_404_error);

  // redirecting all the captive-portal probes produces a lot of "invalid request" noise
  esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
  esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
  esp_log_level_set("httpd_parse", ESP_LOG_ERROR);
}



//===================================
//===== start/stop captiveportal ====
//===================================
// start mdns (http://armchair.local) and the dns-server that resolves every
// hostname to this device, which makes the phone open the web-app on its own
// note: has to be called AFTER the access-point was started

void http_start_captivePortal()
{
  ESP_LOGW(TAG, "starting captive portal (mdns '%s.local' + dns-redirect)...", MDNS_HOSTNAME);
  esp_err_t err = mdns_init();
  if (err != ESP_OK)
    ESP_LOGE(TAG, "failed initializing mdns: %s", esp_err_to_name(err));
  else
  {
    mdns_hostname_set(MDNS_HOSTNAME);
    mdns_instance_name_set("electric armchair");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  }

  dns_server_start();
}

void http_stop_captivePortal()
{
  ESP_LOGW(TAG, "stopping captive portal");
  dns_server_stop();
  mdns_free();
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



