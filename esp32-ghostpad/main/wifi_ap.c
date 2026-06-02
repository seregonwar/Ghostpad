#include "wifi_ap.h"
#include "dns_redirect.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <arpa/inet.h>
#include <string.h>

#if CONFIG_GHOSTPAD_ENABLE_MDNS
#include "mdns.h"
#endif

static const char *TAG = "ghostpad_wifi";

#define GHOSTPAD_AP_IFKEY "WIFI_AP_DEF"
#define DHCPS_OFFER_ROUTER 0x01
#define DHCPS_OFFER_DNS    0x02

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static bool s_sta_connected = false;
static bool s_ap_started = false;
static int s_sta_retry = 0;
static char s_active_ssid[33] = "";

static bool sta_configured(void) {
    return strlen(CONFIG_GHOSTPAD_STA_SSID) > 0;
}

static bool sta_requested(void) {
#if CONFIG_GHOSTPAD_WIFI_MODE_AP_ONLY
    return false;
#else
    return sta_configured();
#endif
}

static bool ap_fallback_allowed(void) {
#if CONFIG_GHOSTPAD_WIFI_MODE_STA_ONLY
    return false;
#else
    return true;
#endif
}

static void configure_ap_dhcp(esp_netif_t *netif) {
    if (!netif) {
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(netif));

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ip_info.gw = ip_info.ip;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_ip_info(netif, &ip_info));

        esp_netif_dns_info_t dns_info = {0};
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        dns_info.ip.u_addr.ip4 = ip_info.ip;
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info));
    }

    uint8_t router_offer = DHCPS_OFFER_ROUTER;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_option(
        netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
        &router_offer,
        sizeof(router_offer)
    ));

    uint8_t dns_offer = DHCPS_OFFER_DNS;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_option(
        netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_DOMAIN_NAME_SERVER,
        &dns_offer,
        sizeof(dns_offer)
    ));

    static const char captive_uri[] = "http://192.168.4.1/";
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_option(
        netif,
        ESP_NETIF_OP_SET,
        ESP_NETIF_CAPTIVEPORTAL_URI,
        (void *)captive_uri,
        strlen(captive_uri)
    ));

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(netif));
}

static void log_netif_ip(const char *label, esp_netif_t *netif) {
    if (!netif) return;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        char ip[16];
        esp_ip4addr_ntoa(&ip_info.ip, ip, sizeof(ip));
        ESP_LOGI(TAG, "%s IP: %s", label, ip);
    }
}

static esp_err_t start_mdns(void) {
#if CONFIG_GHOSTPAD_ENABLE_MDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_hostname_set(CONFIG_GHOSTPAD_WIFI_HOSTNAME));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_instance_name_set("Ghostpad ESP32"));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mdns_service_add("Ghostpad Web UI", "_http", "_tcp", 80, NULL, 0));
    ESP_LOGI(TAG, "mDNS: http://%s.local/", CONFIG_GHOSTPAD_WIFI_HOSTNAME);
#endif
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        if (s_sta_retry < CONFIG_GHOSTPAD_STA_MAX_RETRY) {
            s_sta_retry++;
            ESP_LOGW(TAG, "STA disconnected, retry %d/%d", s_sta_retry, CONFIG_GHOSTPAD_STA_MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_sta_retry = 0;
        s_sta_connected = true;
        strlcpy(s_active_ssid, CONFIG_GHOSTPAD_STA_SSID, sizeof(s_active_ssid));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t start_sta(void) {
    if (!sta_configured()) {
        ESP_LOGW(TAG, "STA SSID is empty; skipping LAN client mode");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
        if (!s_wifi_event_group) return ESP_ERR_NO_MEM;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) return ESP_FAIL;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_set_hostname(s_sta_netif, CONFIG_GHOSTPAD_WIFI_HOSTNAME));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_GHOSTPAD_STA_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_GHOSTPAD_STA_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = strlen(CONFIG_GHOSTPAD_STA_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting ESP32 to LAN SSID: %s", CONFIG_GHOSTPAD_STA_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(CONFIG_GHOSTPAD_STA_CONNECT_TIMEOUT_SEC * 1000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "STA connected to %s", CONFIG_GHOSTPAD_STA_SSID);
        log_netif_ip("STA", s_sta_netif);
        start_mdns();
        return ESP_OK;
    }

    ESP_LOGW(TAG, "STA connection failed or timed out");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    return ESP_FAIL;
}

static esp_err_t start_ap(void) {
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) return ESP_FAIL;
        configure_ap_dhcp(s_ap_netif);
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = CONFIG_GHOSTPAD_AP_CHANNEL,
        },
    };

    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(dns_redirect_start(GHOSTPAD_AP_IFKEY));

    s_ap_started = true;
    strlcpy(s_active_ssid, WIFI_AP_SSID, sizeof(s_active_ssid));

    ESP_LOGI(TAG, "WiFi AP started. SSID: %s", WIFI_AP_SSID);
    log_netif_ip("AP", s_ap_netif);
    ESP_LOGI(TAG, "Fallback Web GUI: http://192.168.4.1");
    return ESP_OK;
}

esp_err_t wifi_ap_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (sta_requested()) {
        esp_err_t sta_ret = start_sta();
        if (sta_ret == ESP_OK) {
            return ESP_OK;
        }

        if (!ap_fallback_allowed()) {
            return sta_ret;
        }

        ESP_LOGW(TAG, "Falling back to Ghostpad SoftAP");
        return start_ap();
    }

    return start_ap();
}

bool ghostpad_wifi_is_sta_connected(void) {
    return s_sta_connected;
}

bool ghostpad_wifi_is_ap_started(void) {
    return s_ap_started;
}

const char *ghostpad_wifi_mode_name(void) {
    if (s_sta_connected) return "sta";
    if (s_ap_started) return "ap";
    return "offline";
}

const char *ghostpad_wifi_active_ssid(void) {
    return s_active_ssid;
}

esp_netif_t *ghostpad_wifi_get_sta_netif(void) {
    return s_sta_netif;
}

esp_netif_t *ghostpad_wifi_get_ap_netif(void) {
    return s_ap_netif;
}

esp_err_t ghostpad_wifi_get_primary_ip(char *buf, size_t len) {
    if (!buf || len == 0) return ESP_ERR_INVALID_ARG;

    esp_netif_t *netif = s_sta_connected ? s_sta_netif : s_ap_netif;
    if (!netif) return ESP_FAIL;

    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK) return err;

    esp_ip4addr_ntoa(&ip_info.ip, buf, len);
    return ESP_OK;
}

int ghostpad_wifi_get_ap_client_count(void) {
    if (!s_ap_started) return 0;

    wifi_sta_list_t sta_list;
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    return (err == ESP_OK) ? sta_list.num : 0;
}
