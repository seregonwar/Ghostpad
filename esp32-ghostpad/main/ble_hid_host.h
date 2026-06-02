// SPDX-License-Identifier: GPL-3.0-or-later
//
//  ____  _     _____   _   _ ___ ____    _   _           _
// | __ )| |   | ____| | | | |_ _|  _ \  | | | | ___  ___| |_
// |  _ \| |   |  _|   | |_| || || | | | | |_| |/ _ \/ __| __|
// | |_) | |___| |___  |  _  || || |_| | |  _  | (_) \__ \ |_
// |____/|_____|_____| |_| |_|___|____/  |_| |_|\___/|___/\__|
//

#pragma once
#include "esp_err.h"
#include <stdbool.h>

/*
 * =====================================================================================
 *                          PUBLIC API — BLE HID Host Module
 *
 *  Manages a NimBLE central role that scans for and connects to BLE HID
 *  gamepads (DualSense VID=054C:0CE6, DualShock4 VID=054C:09CC/05C4).
 *  Input reports are decoded and written into g_gamepad (web_server.h).
 * =====================================================================================
 */

// Initialize NimBLE stack and register callbacks. Call once at startup.
esp_err_t ble_hid_host_init(void);

// True once NimBLE host is synced with BT controller and ready to scan/connect.
bool ble_hid_host_is_synced(void);

// Start BLE scan. Returns ESP_ERR_INVALID_STATE if host not synced or connected.
esp_err_t ble_hid_host_start_scan(void);

// Stop an ongoing scan immediately.
void ble_hid_host_stop_scan(void);

// Clear the internal scan result list.
void ble_hid_host_clear_results(void);

// Return a heap-allocated JSON string of discovered devices.
// Caller MUST free() the returned pointer.
// Format: [{"addr":"xx:xx:xx:xx:xx:xx","name":"...","rssi":-60}, ...]
char *ble_hid_host_get_scan_results_json(void);

// Return heap-allocated JSON with BLE host diagnostics; caller frees it.
char *ble_hid_host_get_debug_json(void);

// Connect to a device by its BLE address string (e.g. "aa:bb:cc:dd:ee:ff").
// Kicks off GATT service discovery and HID report subscription automatically.
esp_err_t ble_hid_host_connect(const char *addr_str);

// Disconnect the currently connected gamepad.
void ble_hid_host_disconnect(void);

// True when a gamepad is connected and sending reports.
bool ble_hid_host_is_connected(void);

// Human-readable name of the connected controller ("" if none).
const char *ble_hid_host_device_name(void);

// BLE address of the connected controller ("" if none).
const char *ble_hid_host_device_addr(void);
