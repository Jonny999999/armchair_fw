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

// whether the user confirmed the sign-in on the portal page (see handleCaptivePortalProbe)
static bool portalSignInDone = false;


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


//====================================
//===== isConnectivityProbe ==========
//====================================
// Whether the requested url is one of the well-known endpoints an operating system polls to
// find out whether the network has internet access. Everything else that arrives for a
// foreign host is the user actually browsing (e.g. typing "google.com") and gets the web-app.
static bool isConnectivityProbe(const char *uri)
{
    static const char *probeUris[] = {
        "/generate_204", "/gen_204",                          // android
        "/hotspot-detect.html", "/library/test/success.html", // ios, macos
        "/connecttest.txt", "/ncsi.txt",                      // windows
        "/success.txt", "/canonical.html",                    // firefox
        "/nm-check.txt", "/check_network_status.txt",         // networkmanager, gnome
    };
    for (int i = 0; i < (int)(sizeof(probeUris) / sizeof(probeUris[0])); i++)
        if (strncmp(uri, probeUris[i], strlen(probeUris[i])) == 0)
            return true;
    return false;
}


//====================================
//===== handleCaptivePortalProbe =====
//====================================
// Answer one of the connectivity-probes of the phone (they arrive here because the
// dns-server resolves every hostname to this device).
//
// Before the sign-in was confirmed: redirect to the portal page. The phone then shows the
// "sign in to network" notification / opens its captive-portal browser on that page
// -> no need to type "192.168.4.1" manually.
//
// Afterwards: answer like a working internet connection, so the phone marks the network as
// signed in and stops nagging / stops re-opening its captive-portal browser.
static esp_err_t handleCaptivePortalProbe(httpd_req_t *req)
{
    if (!portalSignInDone)
    {
        ESP_LOGI(TAG, "captive-portal probe '%s' -> redirecting to the portal page", req->uri);
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://" AP_IP_STR "/portal");
        // note: ios only detects a captive portal when the response has content, an empty redirect is not sufficient
        httpd_resp_send(req, "Armchair remote control - redirecting...", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "captive-portal probe '%s' -> answering 'connected' (sign-in was confirmed)", req->uri);
    // android and windows expect an empty '204', ios/macos a page containing 'Success'
    if (strstr(req->uri, "generate_204") != NULL || strstr(req->uri, "gen_204") != NULL)
    {
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, NULL, 0);
    }
    else
    {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}


//====================================
//===== handleForeignHostRequest =====
//====================================
// a request that was not meant for this device at all - it only arrives because the
// dns-server hijacks every hostname
static esp_err_t handleForeignHostRequest(httpd_req_t *req)
{
    if (isConnectivityProbe(req->uri))
        return handleCaptivePortalProbe(req);
    // the user is actually browsing -> hand them the web-app
    return redirectToWebApp(req);
}


//====================================
//========== portal page =============
//====================================
// Page the captive-portal window of the phone lands on. That window is not the browser of
// the user but a stripped down webview, which e.g. fires its pull-to-refresh while dragging
// the joystick down and does not let the page disable it. There is no way for a page to open
// the real browser (target=_blank and intent:// urls are both blocked there), so this page
// only hands out the address to copy over manually.
//
// The sign-in is deliberately never confirmed on its own: confirming it closes this window,
// and as long as it is not confirmed the user can always get back here via the notification.
// The "confirm sign-in" link at the bottom does it manually (stops the phone from nagging).
static const char portalPage[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>armchair</title><style>"
    "body{background:#001427;color:#fff;font-family:system-ui,sans-serif;margin:0;"
    "padding:1.5rem;text-align:center}"
    "h1{font-size:1.5rem}p{color:#8fa6bb;line-height:1.5}"
    ".box{display:flex;align-items:center;gap:.5rem;margin:1.5rem 0 .8rem;padding:1rem;"
    "border:1px solid #708d81;border-radius:.5rem;text-align:left}"
    "#url{flex:1;font-family:monospace;font-size:1.25rem;color:#fff;word-break:break-all;"
    "-webkit-user-select:all;user-select:all}"
    "#copy{background:#8d0801;color:#fff;border:none;border-radius:.4rem;padding:.6rem .8rem;"
    "font-size:1.2rem;line-height:1;cursor:pointer}"
    ".tip{font-size:.85rem}a{color:#708d81}b{color:#fff}"
    "</style></head><body>"
    "<h1>Electric armchair</h1>"
    "<p>Open this in your browser:</p>"
    "<div class=\"box\"><span id=\"url\">http://armchair.local</span>"
    "<button id=\"copy\" title=\"copy\">&#128203;</button></div>"
    "<p class=\"tip\">Long-press to select. <b>http://" AP_IP_STR "</b> works too.<br>"
    "This window is not a real browser - the joystick misbehaves in it.</p>"
    "<p><a href=\"http://" AP_IP_STR "/\">open it here anyway</a></p>"
    "<p class=\"tip\"><a id=\"done\" href=\"#\">confirm sign-in</a> (stops the "
    "&bdquo;sign in to wifi&ldquo; notification, closes this window)</p>"
    "<script>"
    // navigator.clipboard is unavailable over plain http -> select + execCommand.
    // Leaves the address selected either way, which is what the user wants anyway.
    "document.getElementById('copy').onclick=function(){"
    "var r=document.createRange();r.selectNodeContents(document.getElementById('url'));"
    "var s=getSelection();s.removeAllRanges();s.addRange(r);"
    "var ok=false;try{ok=document.execCommand('copy');}catch(e){}"
    "this.innerHTML=ok?'&#10003;':'&#128203;';};"
    "document.getElementById('done').onclick=function(){"
    "fetch('/portal/done').catch(function(){});};"
    "</script></body></html>";

static esp_err_t on_portal_page(httpd_req_t *req)
{
    ESP_LOGI(TAG, "serving the captive-portal sign-in page");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, portalPage, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

//--- user pressed 'confirm sign-in' ---
static esp_err_t on_portal_done(httpd_req_t *req)
{
    ESP_LOGW(TAG, "sign-in confirmed by the user -> answering further captive-portal probes as 'connected'");
    portalSignInDone = true;
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


//====================================
//===== http_armCaptivePortal ========
//====================================
// Arm the portal again, so the next connectivity-probe gets the sign-in page instead of
// "connected". Called whenever a station joins the access-point (see wifi.c): a phone that
// signed in once otherwise never got the sign-in page again when re-connecting, because the
// probes kept being answered as 'connected' until HTTP mode was left entirely.
extern "C" void http_armCaptivePortal(void)
{
    if (portalSignInDone)
        ESP_LOGW(TAG, "station (re)connected -> showing the sign-in page again on the next probe");
    portalSignInDone = false;
}


//=================================
//===== on_404_error ==============
//=================================
// redirect anything the server does not handle to the web-app as well
static esp_err_t on_404_error(httpd_req_t *req, httpd_err_code_t err)
{
    if (!isRequestForOwnHost(req))
        return handleForeignHostRequest(req);
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
    // requests for foreign hosts only arrive here because of the hijacked dns
    if (!isRequestForOwnHost(req))
        return handleForeignHostRequest(req);

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



//====================================
//===== chair adjustment endpoint ====
//====================================
// endpoint for controlling the leg- and back-rest from the web-app
// (previously only possible locally via encoder or ADJUST_CHAIR mode)
//
//   POST /api/chair   {"rest":"leg"|"back", "action":"up"|"down"|"stop"}   hold-button
//   POST /api/chair   {"rest":"leg"|"back", "percent":0-100}               move to position
//   GET  /api/chair   -> {"leg":{"percent":..,"target":..,"state":".."}, "back":{...}}
//
// note: sending 100/0 again while already at that position is intentionally not
// ignored - it re-runs the motor into the limit switch to re-sync the tracked position

//--- local variables ---
//config with the objects/functions the endpoints operate on (set in http_init_server)
static http_config_t config_l = {};

//----------------------------
//----- restFromJsonItem -----
//----------------------------
// get the rest object the request refers to ("leg" or "back")
static cControlledRest *getRestFromName(const char *name)
{
    if (name == NULL)
        return NULL;
    if (strcasecmp(name, "leg") == 0)
        return config_l.legRest;
    if (strcasecmp(name, "back") == 0)
        return config_l.backRest;
    return NULL;
}


//--------------------------
//--- on_chairAdjust_post --
//--------------------------
static esp_err_t on_chairAdjust_post(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    //--- get data from http request ---
    char buffer[100];
    memset(&buffer, 0, sizeof(buffer));
    if (req->content_len >= sizeof(buffer))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "payload too large");
        return ESP_OK;
    }
    httpd_req_recv(req, buffer, req->content_len);
    ESP_LOGD(TAG, "/api/chair: received data: %s", buffer);

    //--- parse json ---
    cJSON *payload = cJSON_Parse(buffer);
    if (payload == NULL)
    {
        ESP_LOGE(TAG, "/api/chair: failed parsing json '%s'", buffer);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_OK;
    }

    //--- which rest? ---
    cJSON *rest_json = cJSON_GetObjectItem(payload, "rest");
    cControlledRest *rest = getRestFromName(cJSON_IsString(rest_json) ? rest_json->valuestring : NULL);
    if (rest == NULL)
    {
        ESP_LOGE(TAG, "/api/chair: item 'rest' missing or not 'leg'/'back'");
        cJSON_Delete(payload);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "item 'rest' has to be 'leg' or 'back'");
        return ESP_OK;
    }

    //--- run requested action ---
    cJSON *action_json = cJSON_GetObjectItem(payload, "action");
    cJSON *percent_json = cJSON_GetObjectItem(payload, "percent");
    esp_err_t result = ESP_OK;

    // move to a certain position
    if (cJSON_IsNumber(percent_json))
    {
        ESP_LOGI(TAG, "/api/chair: [%s] set target position to %.1f%%", rest->getName(), percent_json->valuedouble);
        rest->setTargetPercent((float)percent_json->valuedouble);
    }
    // hold-button pressed/released (move until stopped or limit reached)
    else if (cJSON_IsString(action_json))
    {
        const char *action = action_json->valuestring;
        ESP_LOGI(TAG, "/api/chair: [%s] action '%s'", rest->getName(), action);
        if (strcasecmp(action, "up") == 0)
            rest->setTargetPercent(100);
        else if (strcasecmp(action, "down") == 0)
            rest->setTargetPercent(0);
        else if (strcasecmp(action, "stop") == 0)
            rest->requestStateChange(REST_OFF);
        else
        {
            ESP_LOGE(TAG, "/api/chair: unknown action '%s'", action);
            result = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "/api/chair: neither 'action' nor 'percent' provided");
        result = ESP_FAIL;
    }

    cJSON_Delete(payload);

    if (result != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "provide 'percent' (0-100) or 'action' (up/down/stop)");
        return ESP_OK;
    }

    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}


//-------------------------
//--- on_chairAdjust_get --
//-------------------------
// current position of both rests, used by the web-app to show the actual position live
static esp_err_t on_chairAdjust_get(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");

    char response[220];
    snprintf(response, sizeof(response),
             "{\"leg\":{\"percent\":%.1f,\"target\":%.1f,\"state\":\"%s\"},"
             "\"back\":{\"percent\":%.1f,\"target\":%.1f,\"state\":\"%s\"}}",
             config_l.legRest->getPercent(), config_l.legRest->getTargetPercent(), restStateStr[config_l.legRest->getState()],
             config_l.backRest->getPercent(), config_l.backRest->getTargetPercent(), restStateStr[config_l.backRest->getState()]);

    httpd_resp_sendstr(req, response);
    return ESP_OK;
}



//===============================
//===== settings endpoint =======
//===============================
// lets the web-app read and change the same 'max duty' setting as the encoder-menu
// (limits the top speed - often changed to get finer control in tight spaces)
//
//   GET  /api/settings   -> {"maxDuty":65}
//   POST /api/settings   {"maxDuty":65}
//
// note: the value is stored in nvs by control.cpp -> only send it when actually
// changed (e.g. when the slider is released), not while dragging

#define MAX_DUTY_MIN 1   // same range as in the encoder-menu (menu.cpp)
#define MAX_DUTY_MAX 100

//-----------------------
//--- on_settings_get ---
//-----------------------
static esp_err_t on_settings_get(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");

    char response[64];
    snprintf(response, sizeof(response), "{\"maxDuty\":%.0f}", config_l.getMaxDuty());
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}


//------------------------
//--- on_settings_post ---
//------------------------
static esp_err_t on_settings_post(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    //--- get data from http request ---
    char buffer[100];
    memset(&buffer, 0, sizeof(buffer));
    size_t receiveLen = req->content_len < sizeof(buffer) - 1 ? req->content_len : sizeof(buffer) - 1;
    httpd_req_recv(req, buffer, receiveLen);
    ESP_LOGD(TAG, "/api/settings: received data: %s", buffer);

    //--- parse json ---
    cJSON *payload = cJSON_Parse(buffer);
    if (payload == NULL)
    {
        ESP_LOGE(TAG, "/api/settings: failed parsing json '%s'", buffer);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_OK;
    }

    //--- apply max duty ---
    cJSON *maxDuty_json = cJSON_GetObjectItem(payload, "maxDuty");
    if (!cJSON_IsNumber(maxDuty_json))
    {
        ESP_LOGE(TAG, "/api/settings: item 'maxDuty' missing or not a number");
        cJSON_Delete(payload);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "expecting number 'maxDuty'");
        return ESP_OK;
    }

    float maxDuty = (float)maxDuty_json->valuedouble;
    cJSON_Delete(payload);

    // limit to the same range the encoder-menu allows
    if (maxDuty < MAX_DUTY_MIN)
        maxDuty = MAX_DUTY_MIN;
    else if (maxDuty > MAX_DUTY_MAX)
        maxDuty = MAX_DUTY_MAX;

    ESP_LOGI(TAG, "/api/settings: setting max duty to %.0f%%", maxDuty);
    // note: also stores the value in nvs and updates the brake thresholds (control.cpp)
    config_l.setMaxDuty(maxDuty);

    httpd_resp_set_status(req, "204 NO CONTENT");
    httpd_resp_send(req, NULL, 0);
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
    // note: limit the length, req->content_len is controlled by the client (was a stack overflow before)
    size_t receiveLen = req->content_len < sizeof(buffer) - 1 ? req->content_len : sizeof(buffer) - 1;
    httpd_req_recv(req, buffer, receiveLen);
    ESP_LOGD(TAG, "/api/joystick: received data: %s", buffer);

    //--- parse received json string to json object ---
    cJSON *payload = cJSON_Parse(buffer);
    if (payload == NULL)
    {
        ESP_LOGE(TAG, "/api/joystick: failed parsing json '%s'", buffer);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_OK;
    }

    //--- extract relevant items from json object ---
    cJSON *x_json = cJSON_GetObjectItem(payload, "x");  
    cJSON *y_json = cJSON_GetObjectItem(payload, "y");  

    //--- verify received data ---
    // note: everything connected to the ap can send requests here, dont crash on malformed data
    if (!cJSON_IsNumber(x_json) || !cJSON_IsNumber(y_json))
    {
        ESP_LOGE(TAG, "/api/joystick: items 'x' and 'y' missing or not numbers: '%s'", buffer);
        cJSON_Delete(payload);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "expecting numbers 'x' and 'y'");
        return ESP_OK;
    }

    //--- save items to struct ---
    joystickData_t data = { };

    //convert json double to float (note: the web-app has to send actual numbers, not strings)
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
void http_init_server(http_config_t config_f)
{
  config_l = config_f;

  ESP_LOGI(TAG, "initializing HTTP-Server...");

  // note: spiffs (webroot) is mounted once at startup in main.cpp and stays mounted
  // (was previously mounted + unmounted again on every single request)

  //---- configure webserver ----
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 6144;      // larger file-buffer is used in on_default_url()
  config.max_uri_handlers = 10;
  config.lru_purge_enable = true; // captive-portal probes open many connections - close the oldest instead of failing

  //---- start webserver ----
  ESP_ERROR_CHECK(httpd_start(&server, &config));


  //----- define URLs -----
  //note: dont use separate assignment of elements because causes controller crash
    httpd_uri_t joystick_url = {
      .uri = "/api/joystick",
      .method = HTTP_POST,
      .handler = config_f.onJoystickUrl,
      };
  httpd_register_uri_handler(server, &joystick_url);

    httpd_uri_t chairAdjust_post_url = {
      .uri = "/api/chair",
      .method = HTTP_POST,
      .handler = on_chairAdjust_post,
      };
  httpd_register_uri_handler(server, &chairAdjust_post_url);

    httpd_uri_t chairAdjust_get_url = {
      .uri = "/api/chair",
      .method = HTTP_GET,
      .handler = on_chairAdjust_get,
      };
  httpd_register_uri_handler(server, &chairAdjust_get_url);

    httpd_uri_t settings_post_url = {
      .uri = "/api/settings",
      .method = HTTP_POST,
      .handler = on_settings_post,
      };
  httpd_register_uri_handler(server, &settings_post_url);

    httpd_uri_t settings_get_url = {
      .uri = "/api/settings",
      .method = HTTP_GET,
      .handler = on_settings_get,
      };
  httpd_register_uri_handler(server, &settings_get_url);

    httpd_uri_t portal_url = {
      .uri = "/portal",
      .method = HTTP_GET,
      .handler = on_portal_page,
      };
  httpd_register_uri_handler(server, &portal_url);

    httpd_uri_t portal_done_url = {
      .uri = "/portal/done",
      .method = HTTP_GET,
      .handler = on_portal_done,
      };
  httpd_register_uri_handler(server, &portal_done_url);

  // note: the wildcard handler has to be registered LAST, the first matching handler wins
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
  portalSignInDone = false; // show the portal page again until the user confirms the sign-in
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



