#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi.h"



//--- variables used for ap and wifi ---
static const char *TAG = "wifi";
static esp_event_handler_instance_t instance_any_id;


//============================================
//============ init nvs and netif ============
//============================================
//initialize nvs-flash and netif (needed for both AP and CLIENT)
//has to be run once at startup 
void wifi_initNvs_initNetif(){
    //Initialize NVS (needed for wifi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}


//===========================================
//============ init access point ============
//===========================================

//--------------------------------------------
//------ configuration / declarations --------
//--------------------------------------------
#define EXAMPLE_ESP_WIFI_SSID_AP      "armchair"
#define EXAMPLE_ESP_WIFI_PASS_AP      ""
#define EXAMPLE_ESP_WIFI_CHANNEL_AP   1
#define EXAMPLE_MAX_STA_CONN_AP       4

static esp_netif_t *ap;

static void wifi_event_handler_ap(void* arg, esp_event_base_t event_base,
        int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                MAC2STR(event->mac), event->aid);
    }
}

//-----------------------
//------ init ap --------
//-----------------------
void wifi_init_ap(void)
{
    ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &wifi_event_handler_ap,
                NULL,
                &instance_any_id));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID_AP,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID_AP),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL_AP,
            .password = EXAMPLE_ESP_WIFI_PASS_AP,
            .max_connection = EXAMPLE_MAX_STA_CONN_AP,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS_AP) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
            EXAMPLE_ESP_WIFI_SSID_AP, EXAMPLE_ESP_WIFI_PASS_AP, EXAMPLE_ESP_WIFI_CHANNEL_AP);
}



//=============================
//========= deinit AP =========
//=============================
void wifi_deinit_ap(void)
{
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy_default_wifi(ap);

}












//===========================================
//=============== init client ===============
//===========================================

//--------------------------------------------
//------ configuration / declarations --------
//--------------------------------------------
#define EXAMPLE_ESP_WIFI_SSID_CLIENT      "BKA-network"
#define EXAMPLE_ESP_WIFI_PASS_CLIENT      "airwaveslogitech410"
#define EXAMPLE_ESP_MAXIMUM_RETRY_CLIENT  10

static esp_netif_t *sta;
static esp_event_handler_instance_t instance_got_ip;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static void event_handler(void* arg, esp_event_base_t event_base,
        int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY_CLIENT) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
//---------------------------
//------ init client --------
//---------------------------
void wifi_init_client(void)
{
    s_wifi_event_group = xEventGroupCreate();
    sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &event_handler,
                NULL,
                &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &event_handler,
                NULL,
                &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID_CLIENT,
            .password = EXAMPLE_ESP_WIFI_PASS_CLIENT,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                EXAMPLE_ESP_WIFI_SSID_CLIENT, EXAMPLE_ESP_WIFI_PASS_CLIENT);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                EXAMPLE_ESP_WIFI_SSID_CLIENT, EXAMPLE_ESP_WIFI_PASS_CLIENT);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    //    /* The event will not be processed after unregister */
    //    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    //    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    //    vEventGroupDelete(s_wifi_event_group);
}



//=================================
//========= deinit client =========
//=================================
void wifi_deinit_client(void)
{
    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy_default_wifi(sta);
}

