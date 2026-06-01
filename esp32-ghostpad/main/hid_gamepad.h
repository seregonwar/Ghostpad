#pragma once
#include "esp_err.h"

esp_err_t hid_gamepad_init(void);
void hid_gamepad_send_report(void);
extern char g_connected_console[32];
