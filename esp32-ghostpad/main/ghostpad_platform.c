#include "sdkconfig.h"
#if CONFIG_BLUEPAD32_PLATFORM_CUSTOM

#include <string.h>
#include <uni.h>
#include "web_server.h"
#include "esp_log.h"

static const char *TAG = "ghostpad_bp32";

static bool s_connected = false;
static char s_device_name[64] = "";

static void bp32_init(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    ESP_LOGI(TAG, "Bluepad32 platform init");
}

static void bp32_on_init_complete(void) {
    ESP_LOGI(TAG, "Bluepad32 init complete, starting scan");
    uni_bt_start_scanning_and_autoconnect_unsafe();
    uni_bt_allow_incoming_connections(true);
}

static uni_error_t bp32_on_device_discovered(bd_addr_t addr, const char *name,
                                              uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)rssi;
    /* Accept all gamepads, filter keyboards */
    if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
        return UNI_ERROR_IGNORE_DEVICE;
    }
    if (name) {
        ESP_LOGI(TAG, "Discovered: %s", name);
    }
    return UNI_ERROR_SUCCESS;
}

static void bp32_on_device_connected(uni_hid_device_t *d) {
    ESP_LOGI(TAG, "Device connected: %p", (void *)d);
}

static void bp32_on_device_disconnected(uni_hid_device_t *d) {
    ESP_LOGI(TAG, "Device disconnected: %p", (void *)d);
    s_connected = false;
    strcpy(s_device_name, "");
    
    gamepad_state_t neutral_gp = {
        .buttons = 0,
        .lx = 128, .ly = 128,
        .rx = 128, .ry = 128,
        .l2 = 0, .r2 = 0
    };
    web_server_handle_physical_gamepad(&neutral_gp);
}

static uni_error_t bp32_on_device_ready(uni_hid_device_t *d) {
    ESP_LOGI(TAG, "Device ready: %p", (void *)d);
    s_connected = true;
    if (d->name[0]) {
        strlcpy(s_device_name, d->name, sizeof(s_device_name));
    } else {
        strlcpy(s_device_name, "Controller", sizeof(s_device_name));
    }
    return UNI_ERROR_SUCCESS;
}

static void bp32_on_controller_data(uni_hid_device_t *d, uni_controller_t *ctl) {
    (void)d;
    if (ctl->klass != UNI_CONTROLLER_CLASS_GAMEPAD) return;

    uni_gamepad_t *gp = &ctl->gamepad;
    s_connected = true;

    gamepad_state_t local_gp;
    local_gp.lx = (uint8_t)(128 + gp->axis_x / 4);
    local_gp.ly = (uint8_t)(128 - gp->axis_y / 4);
    local_gp.rx = (uint8_t)(128 + gp->axis_rx / 4);
    local_gp.ry = (uint8_t)(128 - gp->axis_ry / 4);
    local_gp.l2 = (uint8_t)(gp->brake / 4);
    local_gp.r2 = (uint8_t)(gp->throttle / 4);

    uint32_t buttons = 0;
    if (gp->buttons & BUTTON_A)       buttons |= (1u << 0);  /* Cross    */
    if (gp->buttons & BUTTON_X)       buttons |= (1u << 1);  /* Square   */
    if (gp->buttons & BUTTON_B)       buttons |= (1u << 2);  /* Circle   */
    if (gp->buttons & BUTTON_Y)       buttons |= (1u << 3);  /* Triangle */
    if (gp->dpad & DPAD_UP)           buttons |= (1u << 4);
    if (gp->dpad & DPAD_RIGHT)        buttons |= (1u << 5);
    if (gp->dpad & DPAD_DOWN)         buttons |= (1u << 6);
    if (gp->dpad & DPAD_LEFT)         buttons |= (1u << 7);
    if (gp->buttons & BUTTON_SHOULDER_L)  buttons |= (1u << 8);   /* L1 */
    if (gp->buttons & BUTTON_SHOULDER_R)  buttons |= (1u << 9);   /* R1 */
    if (gp->buttons & BUTTON_THUMB_L)      buttons |= (1u << 10);  /* L3 */
    if (gp->buttons & BUTTON_THUMB_R)      buttons |= (1u << 11);  /* R3 */
    if (gp->buttons & BUTTON_TRIGGER_L)    buttons |= (1u << 12);  /* L2 btn */
    if (gp->buttons & BUTTON_TRIGGER_R)    buttons |= (1u << 13);  /* R2 btn */
    if (gp->misc_buttons & MISC_BUTTON_START)    buttons |= (1u << 14);  /* Options */
    if (gp->misc_buttons & MISC_BUTTON_SELECT)   buttons |= (1u << 15);  /* Create */
    if (gp->misc_buttons & MISC_BUTTON_SYSTEM)   buttons |= (1u << 16);  /* PS */
    if (gp->misc_buttons & MISC_BUTTON_CAPTURE)  buttons |= (1u << 17);  /* Touchpad/Mute */

    local_gp.buttons = buttons;

    web_server_handle_physical_gamepad(&local_gp);
}

static const uni_property_t *bp32_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void bp32_on_oob_event(uni_platform_oob_event_t event, void *data) {
    (void)data;
    switch (event) {
    case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
        ESP_LOGI(TAG, "Bluetooth enabled");
        break;
    default:
        break;
    }
}

struct uni_platform *get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "ghostpad",
        .init = bp32_init,
        .on_init_complete = bp32_on_init_complete,
        .on_device_discovered = bp32_on_device_discovered,
        .on_device_connected = bp32_on_device_connected,
        .on_device_disconnected = bp32_on_device_disconnected,
        .on_device_ready = bp32_on_device_ready,
        .on_oob_event = bp32_on_oob_event,
        .on_controller_data = bp32_on_controller_data,
        .get_property = bp32_get_property,
    };
    return &plat;
}

/* ---- Bridge API for web_server.c ---- */

#include "bt_bridge.h"
#include "cJSON.h"

bool bt_bridge_is_connected(void) {
    return s_connected;
}

const char *bt_bridge_device_name(void) {
    return s_device_name;
}

esp_err_t bt_bridge_init(void) {
    /* Bluepad32 is initialized in main.c via btstack_init() + uni_init().
     * This function is retained for API compatibility but does nothing. */
    ESP_LOGI(TAG, "bt_bridge_init (Bluepad32)");
    return ESP_OK;
}

esp_err_t bt_bridge_start_scan(void) {
    if (s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Bluepad32 handles scanning automatically via uni_bt_start_scanning_and_autoconnect_unsafe() */
    uni_bt_start_scanning_and_autoconnect_unsafe();
    ESP_LOGI(TAG, "BT scan requested via Bluepad32");
    return ESP_OK;
}

void bt_bridge_disconnect(void) {
    /* Bluepad32 manages disconnections internally */
}

char *bt_bridge_get_scan_results_json(void) {
    cJSON *arr = cJSON_CreateArray();
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "name", s_connected ? s_device_name : "Premi PS+Create sul controller");
    cJSON_AddStringToObject(obj, "addr", "ble:auto");
    cJSON_AddStringToObject(obj, "model", "DualSense (Bluepad32)");
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
    cJSON_AddBoolToObject(root, "connected", s_connected);
    cJSON_AddStringToObject(root, "device_name", s_device_name);
    cJSON_AddStringToObject(root, "device_addr", "ble:bluepad32");
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    return json;
}

#endif /* CONFIG_BLUEPAD32_PLATFORM_CUSTOM */
