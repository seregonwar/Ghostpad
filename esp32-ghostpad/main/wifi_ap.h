#pragma once
#include "esp_err.h"

#define WIFI_AP_SSID "Ghostpad-ESP32"
#define WIFI_AP_PASS "ghostpad123"
#define WIFI_AP_MAX_CONN 4

esp_err_t wifi_ap_init(void);
