#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t bt_bridge_init(void);
bool bt_bridge_is_connected(void);
esp_err_t bt_bridge_start_scan(void);
void bt_bridge_disconnect(void);
const char *bt_bridge_device_name(void);
char *bt_bridge_get_scan_results_json(void);
char *bt_bridge_get_debug_json(void);
