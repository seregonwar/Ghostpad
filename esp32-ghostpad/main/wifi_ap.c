#include "wifi_ap.h"
#include "dns_redirect.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "ghostpad_wifi";

#define GHOSTPAD_AP_IFKEY "WIFI_AP_DEF"
#define DHCPS_OFFER_ROUTER 0x01
#define DHCPS_OFFER_DNS    0x02

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

esp_err_t wifi_ap_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();

    configure_ap_dhcp(netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = 1,
        },
    };

    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(dns_redirect_start(GHOSTPAD_AP_IFKEY));

    ESP_LOGI(TAG, "WiFi AP started. SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Connect and open http://192.168.4.1");

    return ESP_OK;
}
