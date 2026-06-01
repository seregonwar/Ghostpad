#include "dns_redirect.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <string.h>

#define DNS_PORT 53
#define DNS_PACKET_MAX_LEN 256
#define DNS_QTYPE_A 1
#define DNS_QCLASS_IN 1

static const char *TAG = "ghostpad_dns";
static TaskHandle_t s_dns_task;
static char s_netif_key[16];

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;
    uint16_t an_count;
    uint16_t ns_count;
    uint16_t ar_count;
} dns_header_t;

typedef struct __attribute__((packed)) {
    uint16_t name_ptr;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t data_len;
    uint32_t address;
} dns_answer_t;

static int skip_dns_name(const uint8_t *packet, int len, int offset) {
    while (offset < len) {
        uint8_t label_len = packet[offset++];
        if (label_len == 0) {
            return offset;
        }
        if ((label_len & 0xC0) == 0xC0) {
            return (offset < len) ? offset + 1 : -1;
        }
        offset += label_len;
    }

    return -1;
}

static int build_dns_reply(const uint8_t *request, int request_len, uint8_t *reply, int reply_max_len, uint32_t ip_addr) {
    if (request_len < (int)sizeof(dns_header_t) || request_len > reply_max_len) {
        return -1;
    }

    memcpy(reply, request, request_len);
    dns_header_t *header = (dns_header_t *)reply;
    uint16_t question_count = ntohs(header->qd_count);
    if (question_count == 0) {
        return -1;
    }

    int question_offset = sizeof(dns_header_t);
    int name_end = skip_dns_name(request, request_len, question_offset);
    if (name_end < 0 || name_end + 4 > request_len) {
        return -1;
    }

    uint16_t qtype;
    uint16_t qclass;
    memcpy(&qtype, request + name_end, sizeof(qtype));
    memcpy(&qclass, request + name_end + sizeof(qtype), sizeof(qclass));
    qtype = ntohs(qtype);
    qclass = ntohs(qclass);

    header->flags = htons(0x8180);
    header->ns_count = 0;
    header->ar_count = 0;

    int reply_len = request_len;
    if (qtype == DNS_QTYPE_A && qclass == DNS_QCLASS_IN && reply_len + (int)sizeof(dns_answer_t) <= reply_max_len) {
        dns_answer_t *answer = (dns_answer_t *)(reply + reply_len);
        answer->name_ptr = htons(0xC000 | question_offset);
        answer->type = htons(DNS_QTYPE_A);
        answer->class = htons(DNS_QCLASS_IN);
        answer->ttl = htonl(60);
        answer->data_len = htons(4);
        answer->address = ip_addr;
        header->an_count = htons(1);
        reply_len += sizeof(dns_answer_t);
    } else {
        header->an_count = 0;
    }

    return reply_len;
}

static uint32_t get_ap_ip_addr(void) {
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(s_netif_key);
    if (!netif) {
        return IPADDR_ANY;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return IPADDR_ANY;
    }

    return ip_info.ip.addr;
}

static void dns_redirect_task(void *arg) {
    (void)arg;

    uint8_t rx_buffer[DNS_PACKET_MAX_LEN];
    uint8_t tx_buffer[DNS_PACKET_MAX_LEN];

    struct sockaddr_in listen_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(DNS_PORT),
    };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        ESP_LOGE(TAG, "Unable to bind DNS socket: errno %d", errno);
        close(sock);
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS redirect server started on UDP port %d", DNS_PORT);

    while (true) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGW(TAG, "DNS recvfrom failed: errno %d", errno);
            continue;
        }

        uint32_t ap_ip = get_ap_ip_addr();
        if (ap_ip == IPADDR_ANY) {
            continue;
        }

        int reply_len = build_dns_reply(rx_buffer, len, tx_buffer, sizeof(tx_buffer), ap_ip);
        if (reply_len <= 0) {
            continue;
        }

        int sent = sendto(sock, tx_buffer, reply_len, 0, (struct sockaddr *)&source_addr, socklen);
        if (sent < 0) {
            ESP_LOGW(TAG, "DNS sendto failed: errno %d", errno);
        }
    }
}

esp_err_t dns_redirect_start(const char *netif_key) {
    if (s_dns_task) {
        return ESP_OK;
    }

    if (!netif_key || netif_key[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    strlcpy(s_netif_key, netif_key, sizeof(s_netif_key));

    BaseType_t ok = xTaskCreate(dns_redirect_task, "ghostpad_dns", 4096, NULL, 5, &s_dns_task);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
