#pragma once
#include "esp_err.h"

typedef struct {
    uint32_t buttons;
    uint8_t  lx;
    uint8_t  ly;
    uint8_t  rx;
    uint8_t  ry;
    uint8_t  l2;
    uint8_t  r2;
} gamepad_state_t;

extern volatile gamepad_state_t g_gamepad;

esp_err_t web_server_start(void);
void web_server_handle_physical_gamepad(const gamepad_state_t *gp);
