#pragma once
#include "esp_err.h"
#include "esp_netif.h"
#include <stdbool.h>
#include <stddef.h>

#define WIFI_AP_SSID CONFIG_GHOSTPAD_AP_SSID
#define WIFI_AP_PASS CONFIG_GHOSTPAD_AP_PASSWORD
#define WIFI_AP_MAX_CONN CONFIG_GHOSTPAD_AP_MAX_CONN

esp_err_t wifi_ap_init(void);

bool ghostpad_wifi_is_sta_connected(void);
bool ghostpad_wifi_is_ap_started(void);
const char *ghostpad_wifi_mode_name(void);
const char *ghostpad_wifi_active_ssid(void);
esp_netif_t *ghostpad_wifi_get_sta_netif(void);
esp_netif_t *ghostpad_wifi_get_ap_netif(void);
esp_err_t ghostpad_wifi_get_primary_ip(char *buf, size_t len);
int ghostpad_wifi_get_ap_client_count(void);
