#define BT_CONTROLLER_IMPL
#include "bt_controller.h"
#include "bt_bridge.h"
#include "web_server.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bt_bridge";

static bool s_connected = false;
static char s_device_name[64] = "";

static void on_bt_connection(btc_state_t new_state,
                              btc_controller_type_t ctrl_type,
                              void *user) {
    (void)user;
    switch (new_state) {
    case BTC_STATE_CONNECTED:
        s_connected = true;
        strlcpy(s_device_name,
                ctrl_type == BTC_CTRL_DUALSENSE ? "DualSense" : "BT Controller",
                sizeof(s_device_name));
        ESP_LOGI(TAG, "BT controller connected: %s", s_device_name);
        break;
    case BTC_STATE_IDLE:
    case BTC_STATE_DISCONNECTING:
        s_connected = false;
        strcpy(s_device_name, "");
        ESP_LOGI(TAG, "BT controller disconnected");
        break;
    case BTC_STATE_SCANNING:
        ESP_LOGI(TAG, "BT scanning...");
        break;
    case BTC_STATE_CONNECTING:
        ESP_LOGI(TAG, "BT connecting...");
        break;
    default:
        break;
    }
}

static void on_bt_input(const btc_input_state_t *state, void *user) {
    (void)user;
    if (!state || !state->valid) return;

    g_gamepad.lx = state->lx;
    g_gamepad.ly = state->ly;
    g_gamepad.rx = state->rx;
    g_gamepad.ry = state->ry;
    g_gamepad.l2 = state->l2;
    g_gamepad.r2 = state->r2;

    uint32_t buttons = 0;
    if (state->btn_cross)    buttons |= (1u << 0);
    if (state->btn_square)   buttons |= (1u << 1);
    if (state->btn_circle)   buttons |= (1u << 2);
    if (state->btn_triangle) buttons |= (1u << 3);
    if (state->dpad == BTC_DPAD_N || state->dpad == BTC_DPAD_NE || state->dpad == BTC_DPAD_NW)
                             buttons |= (1u << 4);
    if (state->dpad == BTC_DPAD_E || state->dpad == BTC_DPAD_NE || state->dpad == BTC_DPAD_SE)
                             buttons |= (1u << 5);
    if (state->dpad == BTC_DPAD_S || state->dpad == BTC_DPAD_SE || state->dpad == BTC_DPAD_SW)
                             buttons |= (1u << 6);
    if (state->dpad == BTC_DPAD_W || state->dpad == BTC_DPAD_NW || state->dpad == BTC_DPAD_SW)
                             buttons |= (1u << 7);
    if (state->btn_l1)      buttons |= (1u << 8);
    if (state->btn_r1)      buttons |= (1u << 9);
    if (state->btn_l3)      buttons |= (1u << 10);
    if (state->btn_r3)      buttons |= (1u << 11);
    if (state->btn_l2)      buttons |= (1u << 12);
    if (state->btn_r2)      buttons |= (1u << 13);
    if (state->btn_options) buttons |= (1u << 14);
    if (state->btn_create)  buttons |= (1u << 15);
    if (state->btn_ps)      buttons |= (1u << 16);
    if (state->btn_touchpad)buttons |= (1u << 17);
    if (state->btn_mute)    buttons |= (1u << 18);

    g_gamepad.buttons = buttons;
}

esp_err_t bt_bridge_init(void) {
    btc_config_t cfg = {
        .on_input       = on_bt_input,
        .on_connection  = on_bt_connection,
        .user_data      = NULL,
        .auto_reconnect = true,
        .scan_timeout_ms = 15000,
    };
    btc_err_t ret = btc_lib_init(&cfg);
    if (ret != BTC_OK) {
        ESP_LOGE(TAG, "btc_lib_init failed: %d", ret);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BT Bridge initialized (Bluedroid + HID Host v%s)", btc_version_string());
    return ESP_OK;
}

bool bt_bridge_is_connected(void) {
    return s_connected;
}

esp_err_t bt_bridge_start_scan(void) {
    if (s_connected) {
        ESP_LOGW(TAG, "Already connected, skipping scan");
        return ESP_ERR_INVALID_STATE;
    }
    btc_err_t ret = btc_scan_start();
    if (ret != BTC_OK) {
        ESP_LOGE(TAG, "btc_scan_start failed: %d", ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void bt_bridge_disconnect(void) {
    btc_disconnect();
}

const char *bt_bridge_device_name(void) {
    return s_device_name;
}

char *bt_bridge_get_scan_results_json(void) {
    cJSON *arr = cJSON_CreateArray();
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "name", s_connected ? s_device_name : "Premi PS+Create sul DualSense");
    cJSON_AddStringToObject(obj, "addr", "bt:classic");
    cJSON_AddStringToObject(obj, "model", "DualSense (BT Classic)");
    cJSON_AddBoolToObject(obj, "candidate", true);
    cJSON_AddBoolToObject(obj, "sony_name", true);
    cJSON_AddNumberToObject(obj, "score", 90);
    cJSON_AddItemToArray(arr, obj);
    char *json = cJSON_Print(arr);
    cJSON_Delete(arr);
    return json;
}

char *bt_bridge_get_debug_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "synced", true);
    cJSON_AddBoolToObject(root, "scanning", btc_get_connection_state() == BTC_STATE_SCANNING);
    cJSON_AddBoolToObject(root, "connected", s_connected);
    cJSON_AddStringToObject(root, "device_name", s_device_name);
    cJSON_AddStringToObject(root, "device_addr", "bt:classic");
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    return json;
}
