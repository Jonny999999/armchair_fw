/* Simple DNS server that redirects all queries to the soft-AP address.
 *
 * Based on the ESP-IDF example 'protocols/http_server/captive_portal'
 * (Public Domain / CC0), extended with the ability to stop the server again,
 * since the access-point is only running while in HTTP control-mode.
 */

#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "dns_server.h"


//--- config ---
#define DNS_PORT 53
#define DNS_MAX_LEN 256
#define DNS_RECV_TIMEOUT_MS 500 // interval the task checks whether it should exit
#define ANS_TTL_SEC 300

#define OPCODE_MASK 0x7800
#define QR_FLAG (1 << 7)
#define QD_TYPE_A 0x0001


//--- local variables ---
static const char *TAG = "dns-server";
static TaskHandle_t dnsTaskHandle = NULL;
static volatile bool keepRunning = false;
static int dnsSocket = -1;


// DNS Header Packet
typedef struct __attribute__((__packed__))
{
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

// DNS Question Packet
typedef struct {
    uint16_t type;
    uint16_t class;
} dns_question_t;

// DNS Answer Packet
typedef struct __attribute__((__packed__))
{
    uint16_t ptr_offset;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t addr_len;
    uint32_t ip_addr;
} dns_answer_t;


//==========================
//===== parse_dns_name =====
//==========================
// parse the name from the DNS name format to a regular .-separated name,
// returns pointer to the next part of the packet
static char *parse_dns_name(char *raw_name, char *parsed_name, size_t parsed_name_max_len)
{
    char *label = raw_name;
    char *name_itr = parsed_name;
    int name_len = 0;

    do {
        int sub_name_len = *label;
        // (len + 1) since we are adding a '.'
        name_len += (sub_name_len + 1);
        if (name_len > parsed_name_max_len)
            return NULL;

        // copy the sub name that follows the label
        memcpy(name_itr, label + 1, sub_name_len);
        name_itr[sub_name_len] = '.';
        name_itr += (sub_name_len + 1);
        label += sub_name_len + 1;
    } while (*label != 0);

    // terminate the final string, replacing the last '.'
    parsed_name[name_len - 1] = '\0';
    return label + 1;
}


//=============================
//===== parse_dns_request =====
//=============================
// parse the DNS request and prepare a DNS response with the IP of the soft-AP
static int parse_dns_request(char *req, size_t req_len, char *dns_reply, size_t dns_reply_max_len)
{
    if (req_len > dns_reply_max_len)
        return -1;

    // prepare the reply
    memset(dns_reply, 0, dns_reply_max_len);
    memcpy(dns_reply, req, req_len);

    // endianess of network packet differs from chip
    dns_header_t *header = (dns_header_t *)dns_reply;
    ESP_LOGD(TAG, "DNS query with header id: 0x%X, flags: 0x%X, qd_count: %d",
             ntohs(header->id), ntohs(header->flags), ntohs(header->qd_count));

    // not a standard query
    if ((header->flags & OPCODE_MASK) != 0)
        return 0;

    // set question response flag
    header->flags |= QR_FLAG;

    uint16_t qd_count = ntohs(header->qd_count);
    header->an_count = htons(qd_count);

    int reply_len = qd_count * sizeof(dns_answer_t) + req_len;
    if (reply_len > dns_reply_max_len)
        return -1;

    // pointer to current answer and question
    char *cur_ans_ptr = dns_reply + req_len;
    char *cur_qd_ptr = dns_reply + sizeof(dns_header_t);
    char name[128];

    // respond to all questions with the ESP32's IP address
    for (int i = 0; i < qd_count; i++) {
        char *name_end_ptr = parse_dns_name(cur_qd_ptr, name, sizeof(name));
        if (name_end_ptr == NULL) {
            ESP_LOGE(TAG, "Failed to parse DNS question: %s", cur_qd_ptr);
            return -1;
        }

        dns_question_t *question = (dns_question_t *)(name_end_ptr);
        uint16_t qd_type = ntohs(question->type);
        uint16_t qd_class = ntohs(question->class);

        ESP_LOGD(TAG, "Received type: %d | Class: %d | Question for: %s", qd_type, qd_class, name);

        if (qd_type == QD_TYPE_A) {
            dns_answer_t *answer = (dns_answer_t *)cur_ans_ptr;

            answer->ptr_offset = htons(0xC000 | (cur_qd_ptr - dns_reply));
            answer->type = htons(qd_type);
            answer->class = htons(qd_class);
            answer->ttl = htonl(ANS_TTL_SEC);

            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
            ESP_LOGD(TAG, "Answer with PTR offset: 0x%X and IP 0x%X", ntohs(answer->ptr_offset), ip_info.ip.addr);

            answer->addr_len = htons(sizeof(ip_info.ip.addr));
            answer->ip_addr = ip_info.ip.addr;
        }
    }
    return reply_len;
}


//============================
//===== dns_server_task ======
//============================
// listen for DNS queries and reply to all type-A queries with the IP of the soft-AP
static void dns_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char reply[DNS_MAX_LEN];

    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    dnsSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (dnsSocket < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        goto exit;
    }

    // timeout on recvfrom, so the task notices when it should stop
    struct timeval timeout = {.tv_sec = 0, .tv_usec = DNS_RECV_TIMEOUT_MS * 1000};
    setsockopt(dnsSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (bind(dnsSocket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto exit;
    }
    ESP_LOGW(TAG, "DNS-server started (port %d) - all hostnames now resolve to the armchair", DNS_PORT);

    while (keepRunning) {
        struct sockaddr_in6 source_addr; // large enough for both IPv4 and IPv6
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(dnsSocket, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            // recv timeout is expected (used to poll keepRunning)
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        rx_buffer[len] = 0;
        int reply_len = parse_dns_request(rx_buffer, len, reply, DNS_MAX_LEN);
        ESP_LOGD(TAG, "Received %d bytes | DNS reply with len: %d", len, reply_len);

        if (reply_len <= 0) {
            ESP_LOGE(TAG, "Failed to prepare a DNS reply");
            continue;
        }
        if (sendto(dnsSocket, reply, reply_len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            break;
        }
    }

exit:
    if (dnsSocket >= 0) {
        shutdown(dnsSocket, 0);
        close(dnsSocket);
        dnsSocket = -1;
    }
    ESP_LOGW(TAG, "DNS-server stopped");
    keepRunning = false;
    dnsTaskHandle = NULL;
    vTaskDelete(NULL);
}


//============================
//===== dns_server_start =====
//============================
void dns_server_start(void)
{
    if (dnsTaskHandle != NULL) {
        ESP_LOGW(TAG, "dns-server already running - not starting again");
        return;
    }
    keepRunning = true;
    xTaskCreate(dns_server_task, "task_dns-server", 3072, NULL, 4, &dnsTaskHandle);
}


//===========================
//===== dns_server_stop =====
//===========================
void dns_server_stop(void)
{
    if (dnsTaskHandle == NULL) {
        ESP_LOGW(TAG, "dns-server not running - nothing to stop");
        return;
    }
    ESP_LOGW(TAG, "stopping dns-server...");
    keepRunning = false;
    // wait for the task to notice and clean up its socket (recv timeout + margin)
    for (int i = 0; i < 20 && dnsTaskHandle != NULL; i++)
        vTaskDelay(100 / portTICK_PERIOD_MS);
    if (dnsTaskHandle != NULL)
        ESP_LOGE(TAG, "dns-server task did not exit in time!");
}
