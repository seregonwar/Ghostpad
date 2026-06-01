// SPDX-License-Identifier: GPL-3.0-or-later
//
//  ____  _     _____   _   _ ___ ____    _   _           _
// | __ )| |   | ____| | | | |_ _|  _ \  | | | | ___  ___| |_
// |  _ \| |   |  _|   | |_| || || | | | | |_| |/ _ \/ __| __|
// | |_) | |___| |___  |  _  || || |_| | |  _  | (_) \__ \ |_
// |____/|_____|_____| |_| |_|___|____/  |_| |_|\___/|___/\__|
//
//  BLE HID Host — connects DualSense / DualShock4 directly to the ESP32-P4.
//  NimBLE host-only stack: HCI frames travel via esp-hosted VHCI over SDIO.
//

#include "ble_hid_host.h"
#include "web_server.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"

#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "ble_hid";

/*
 * =====================================================================================
 *                               CONSTANTS & UUIDS
 *
 *  BLE HID over GATT uses the 16-bit GATT assigned numbers below.
 * =====================================================================================
 *
 *  HID Service                : 0x1812
 *  HID Report Characteristic  : 0x2A4D
 *  HID Report Map             : 0x2A4B
 *  Report Reference Descriptor: 0x2908
 *  Client Char Config Desc    : 0x2902
 *
 *  Sony VID (DualSense/DS4)   : 0x054C
 *  DualSense Product ID       : 0x0CE6
 *  DualShock4 Product IDs     : 0x09CC, 0x05C4
 *
 *  BLE appearance: HID Gamepad = 0x03C4
 */

#define BLE_UUID_HID_SERVICE        0x1812
#define BLE_UUID_HID_REPORT         0x2A4D
#define BLE_UUID_CCCD               0x2902

#define MAX_SCAN_RESULTS            16
#define SCAN_DURATION_MS            6000   /* 6 s active window */

/*
 * =====================================================================================
 *                               SCAN RESULT STORAGE
 * =====================================================================================
 */

typedef struct {
    ble_addr_t  addr;
    char        name[64];
    int8_t      rssi;
    bool        valid;
} scan_result_t;

static scan_result_t s_results[MAX_SCAN_RESULTS];
static int           s_result_count = 0;
static SemaphoreHandle_t s_results_mutex = NULL;

/*
 * =====================================================================================
 *                              CONNECTION STATE
 * =====================================================================================
 */

static uint16_t s_conn_handle  = BLE_HS_CONN_HANDLE_NONE;
static bool     s_connected    = false;
static char     s_device_name[64] = "";
static char     s_device_addr[18] = "";

/* Handle of the HID Report characteristic (input report, notifiable) */
static uint16_t s_report_handle = 0;

/* True while a GATT discovery is running */
static bool s_discovering = false;

/* True once NimBLE host is synced with the controller */
static bool s_synced = false;

/*
 * =====================================================================================
 *                           INPUT REPORT DECODERS
 *
 *  DS5 (DualSense) BLE report ID 0x01 layout (bytes are zero-indexed
 *  after the report-ID byte that NimBLE strips for us):
 *
 *   0: LX   1: LY   2: RX   3: RY
 *   4: L2   5: R2
 *   6-9: button bitmask (little-endian)
 *
 *  DS4 BLE report ID 0x11 layout (first byte after report-id is skipped):
 *
 *   1: LX   2: LY   3: RX   4: RY
 *   6-8: button bytes
 *   9: L2   10: R2
 * =====================================================================================
 */

static void decode_dualsense(const uint8_t *data, uint16_t len) {
    if (len < 10) return;
    /* data[0] already past the report-id byte */
    g_gamepad.lx = data[0];
    g_gamepad.ly = data[1];
    g_gamepad.rx = data[2];
    g_gamepad.ry = data[3];
    g_gamepad.l2 = data[4];
    g_gamepad.r2 = data[5];

    uint32_t raw = (uint32_t)data[6]
                 | ((uint32_t)data[7] << 8)
                 | ((uint32_t)data[8] << 16)
                 | ((uint32_t)data[9] << 24);

    /*
     *  DualSense bit layout → Ghostpad internal bitmask
     *  cross=0, square=4, circle=5, triangle=7
     *  L1=9  L2=10  R1=10  R2=11  create=8  options=9
     *  L3=14 R3=15  ps=16  touchpad=17 up=dpad down=dpad left=dpad right=dpad
     *
     *  DualSense raw word (first 4 bytes of button field):
     *    bit 0  = square    bit 1 = cross    bit 2 = circle   bit 3 = triangle
     *    bit 4  = L1        bit 5 = R1       bit 6 = L2_btn   bit 7 = R2_btn
     *    bit 8  = create    bit 9 = options  bit 10= L3       bit 11= R3
     *    bit 12 = ps        bit 13= touchpad
     *    bits 16-19 = dpad (0=up, 1=upright, 2=right, 3=downright,
     *                       4=down, 5=downleft, 6=left, 7=upleft, 8=neutral)
     */
    uint8_t dpad = (raw >> 16) & 0x0F;

    uint32_t buttons = 0;
    if (raw & (1u << 1))  buttons |= (1u << 0);   /* cross     */
    if (raw & (1u << 0))  buttons |= (1u << 1);   /* square    */
    if (raw & (1u << 2))  buttons |= (1u << 2);   /* circle    */
    if (raw & (1u << 3))  buttons |= (1u << 3);   /* triangle  */
    if (raw & (1u << 4))  buttons |= (1u << 8);   /* L1        */
    if (raw & (1u << 5))  buttons |= (1u << 9);   /* R1        */
    if (raw & (1u << 6))  buttons |= (1u << 12);  /* L2 btn    */
    if (raw & (1u << 7))  buttons |= (1u << 13);  /* R2 btn    */
    if (raw & (1u << 8))  buttons |= (1u << 15);  /* create    */
    if (raw & (1u << 9))  buttons |= (1u << 14);  /* options   */
    if (raw & (1u << 10)) buttons |= (1u << 10);  /* L3        */
    if (raw & (1u << 11)) buttons |= (1u << 11);  /* R3        */
    if (raw & (1u << 12)) buttons |= (1u << 16);  /* PS        */
    if (raw & (1u << 13)) buttons |= (1u << 17);  /* touchpad  */

    /* D-pad encoding: values 0..7 clockwise from up, 8=neutral */
    if (dpad == 0 || dpad == 1 || dpad == 7) buttons |= (1u << 4);  /* up    */
    if (dpad == 2 || dpad == 1 || dpad == 3) buttons |= (1u << 5);  /* right */
    if (dpad == 4 || dpad == 3 || dpad == 5) buttons |= (1u << 6);  /* down  */
    if (dpad == 6 || dpad == 5 || dpad == 7) buttons |= (1u << 7);  /* left  */

    g_gamepad.buttons = buttons;
}

static void decode_dualshock4(const uint8_t *data, uint16_t len) {
    /*
     *  DS4 BLE report 0x11 — first byte after report-id is a counter,
     *  so actual axes start at offset 1 relative to data[].
     */
    if (len < 12) return;
    g_gamepad.lx = data[1];
    g_gamepad.ly = data[2];
    g_gamepad.rx = data[3];
    g_gamepad.ry = data[4];

    uint8_t b0   = data[6];
    uint8_t b1   = data[7];
    uint8_t b2   = data[8];
    g_gamepad.l2 = data[9];
    g_gamepad.r2 = data[10];

    uint8_t dpad = b0 & 0x0F;

    uint32_t buttons = 0;
    if (b0 & 0x20) buttons |= (1u << 1);   /* square    */
    if (b0 & 0x40) buttons |= (1u << 0);   /* cross     */
    if (b0 & 0x80) buttons |= (1u << 2);   /* circle    */
    if (b0 & 0x10) buttons |= (1u << 3);   /* triangle  */
    if (b1 & 0x01) buttons |= (1u << 8);   /* L1        */
    if (b1 & 0x02) buttons |= (1u << 9);   /* R1        */
    if (b1 & 0x04) buttons |= (1u << 12);  /* L2 btn    */
    if (b1 & 0x08) buttons |= (1u << 13);  /* R2 btn    */
    if (b1 & 0x10) buttons |= (1u << 15);  /* share     */
    if (b1 & 0x20) buttons |= (1u << 14);  /* options   */
    if (b1 & 0x40) buttons |= (1u << 10);  /* L3        */
    if (b1 & 0x80) buttons |= (1u << 11);  /* R3        */
    if (b2 & 0x01) buttons |= (1u << 16);  /* PS        */
    if (b2 & 0x02) buttons |= (1u << 17);  /* touchpad  */

    if (dpad == 0 || dpad == 1 || dpad == 7) buttons |= (1u << 4);
    if (dpad == 2 || dpad == 1 || dpad == 3) buttons |= (1u << 5);
    if (dpad == 4 || dpad == 3 || dpad == 5) buttons |= (1u << 6);
    if (dpad == 6 || dpad == 5 || dpad == 7) buttons |= (1u << 7);

    g_gamepad.buttons = buttons;
}


static int notify_cb(uint16_t conn_handle,
                     const struct ble_gatt_error *error,
                     struct ble_gatt_attr *attr, void *arg) {
    (void)arg;
    if (error->status != 0 || attr == NULL) return 0;

    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    if (len == 0) return 0;

    uint8_t *data = malloc(len);
    if (!data) return 0;
    os_mbuf_copydata(attr->om, 0, len, data);

    /* Identify report type by first byte (report-id) */
    uint8_t report_id = data[0];

    if (report_id == 0x01 && len >= 11) {
        /* DualSense: skip report-id byte */
        decode_dualsense(data + 1, len - 1);
    } else if (report_id == 0x11 && len >= 13) {
        /* DualShock 4: skip report-id byte */
        decode_dualshock4(data + 1, len - 1);
    }

    free(data);
    return 0;
}

/*
 * =====================================================================================
 *                        GATT SERVICE DISCOVERY
 *
 *  After connecting we discover the HID service (0x1812), find the
 *  first notifiable Report characteristic (0x2A4D), and enable its CCCD
 *  so the controller sends reports automatically.
 * =====================================================================================
 */

static int on_write_cccd(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr, void *arg) {
    if (error->status == 0) {
        ESP_LOGI(TAG, "CCCD enabled — HID reports active");
    } else {
        ESP_LOGW(TAG, "CCCD write failed: %d", error->status);
    }
    return 0;
}

static int on_disc_chars(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == BLE_HS_EDONE) {
        if (s_report_handle != 0) {
            /* Enable notifications: write 0x0001 to CCCD (handle = report + 1) */
            uint8_t val[2] = {0x01, 0x00};
            ble_gattc_write_flat(conn_handle, s_report_handle + 1,
                                 val, sizeof(val), on_write_cccd, NULL);
        }
        s_discovering = false;
        return 0;
    }
    if (error->status != 0) {
        s_discovering = false;
        return 0;
    }

    /* Look for the HID Report characteristic with notify property */
    if (chr->uuid.u.type == BLE_UUID_TYPE_16) {
        uint16_t uuid16 = chr->uuid.u16.value;
        if (uuid16 == BLE_UUID_HID_REPORT &&
            (chr->properties & BLE_GATT_CHR_PROP_NOTIFY)) {
            s_report_handle = chr->val_handle;
            ESP_LOGI(TAG, "Found HID Report characteristic, handle=0x%04x",
                     s_report_handle);
        }
    }
    return 0;
}

static int on_disc_service(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *svc, void *arg) {
    if (error->status == BLE_HS_EDONE) return 0;
    if (error->status != 0) return 0;

    if (svc->uuid.u.type == BLE_UUID_TYPE_16 &&
        svc->uuid.u16.value == BLE_UUID_HID_SERVICE) {
        ESP_LOGI(TAG, "Found HID Service (0x1812), discovering characteristics...");
        s_discovering = true;
        ble_gattc_disc_all_chrs(conn_handle,
                                svc->start_handle, svc->end_handle,
                                on_disc_chars, NULL);
    }
    return 0;
}

/*
 * =====================================================================================
 *                           GAP EVENT HANDLER
 * =====================================================================================
 */

static void addr_to_str(const ble_addr_t *addr, char *buf, size_t len) {
    snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr->val[5], addr->val[4], addr->val[3],
             addr->val[2], addr->val[1], addr->val[0]);
}

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {

    /* ---- Scan result ---- */
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        int rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                         event->disc.length_data);
        if (rc != 0) break;

        /* Accept only devices that advertise the HID service (0x1812)
         * or the HID gamepad appearance (0x03C4), or the Sony name prefix */
        bool is_hid     = false;
        bool is_gamepad = false;

        if (fields.appearance_is_present &&
            (fields.appearance == 0x03C4 || fields.appearance == 0x03C0)) {
            is_gamepad = true;
        }

        for (int i = 0; i < fields.num_uuids16; i++) {
            if (ble_uuid_u16(&fields.uuids16[i].u) == BLE_UUID_HID_SERVICE) {
                is_hid = true;
                break;
            }
        }

        /* Also match known Sony name prefixes */
        bool is_sony = false;
        if (fields.name && fields.name_len > 0) {
            if (strncmp((char *)fields.name, "DualSense", 9) == 0 ||
                strncmp((char *)fields.name, "Wireless Controller", 19) == 0) {
                is_sony = true;
            }
        }

        if (!is_hid && !is_gamepad && !is_sony) break;

        xSemaphoreTake(s_results_mutex, portMAX_DELAY);
        /* Deduplicate by address */
        bool found = false;
        for (int i = 0; i < s_result_count; i++) {
            if (memcmp(s_results[i].addr.val, event->disc.addr.val, 6) == 0) {
                found = true;
                /* Update RSSI */
                s_results[i].rssi = event->disc.rssi;
                break;
            }
        }
        if (!found && s_result_count < MAX_SCAN_RESULTS) {
            scan_result_t *r = &s_results[s_result_count++];
            r->addr  = event->disc.addr;
            r->rssi  = event->disc.rssi;
            r->valid = true;
            if (fields.name && fields.name_len > 0) {
                size_t n = fields.name_len < (sizeof(r->name) - 1)
                           ? fields.name_len : (sizeof(r->name) - 1);
                memcpy(r->name, fields.name, n);
                r->name[n] = '\0';
            } else {
                strlcpy(r->name, "HID Device", sizeof(r->name));
            }
            char addr_str[18];
            addr_to_str(&r->addr, addr_str, sizeof(addr_str));
            ESP_LOGI(TAG, "Discovered: %s  \"%s\"  RSSI=%d",
                     addr_str, r->name, r->rssi);
        }
        xSemaphoreGive(s_results_mutex);
        break;
    }

    /* ---- Scan complete ---- */
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "BLE scan complete (%d devices found)", s_result_count);
        break;

    /* ---- Connected ---- */
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_connected   = true;
            ESP_LOGI(TAG, "Connected! handle=%d  Discovering HID service...",
                     s_conn_handle);
            s_report_handle = 0;
            ble_gattc_disc_all_svcs(s_conn_handle, on_disc_service, NULL);
        } else {
            ESP_LOGE(TAG, "Connect failed: status=%d", event->connect.status);
            s_connected = false;
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        }
        break;

    /* ---- Disconnected ---- */
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected: reason=%d",
                 event->disconnect.reason);
        s_connected   = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_report_handle = 0;
        strcpy(s_device_name, "");
        strcpy(s_device_addr, "");

        /* Reset gamepad to center/neutral on disconnect */
        g_gamepad.buttons = 0;
        g_gamepad.lx = 128; g_gamepad.ly = 128;
        g_gamepad.rx = 128; g_gamepad.ry = 128;
        g_gamepad.l2 = 0;   g_gamepad.r2 = 0;
        break;

    /* ---- Notify receive (unsolicited HID report) ---- */
    case BLE_GAP_EVENT_NOTIFY_RX: {
        struct ble_gatt_attr attr = {
            .handle = event->notify_rx.attr_handle,
            .om     = event->notify_rx.om,
        };
        notify_cb(event->notify_rx.conn_handle, NULL, &attr, NULL);
        break;
    }

    default:
        break;
    }
    return 0;
}

/*
 * =====================================================================================
 *                              SYNC / RESET CALLBACKS
 * =====================================================================================
 */

static void on_ble_sync(void) {
    ESP_LOGI(TAG, "NimBLE host synced with controller");
    s_synced = true;

    /* Ensure we have an address to use as scanner */
    ble_hs_util_ensure_addr(0);
}

static void on_ble_reset(int reason) {
    ESP_LOGW(TAG, "NimBLE host reset: reason=%d", reason);
    s_synced     = false;
    s_connected   = false;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

static void nimble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/*
 * =====================================================================================
 *                              PUBLIC API IMPLEMENTATION
 * =====================================================================================
 */

esp_err_t ble_hid_host_init(void) {
    s_results_mutex = xSemaphoreCreateMutex();
    if (!s_results_mutex) return ESP_ERR_NO_MEM;

    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ble_hs_cfg.sync_cb  = on_ble_sync;
    ble_hs_cfg.reset_cb = on_ble_reset;

    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE HID Host initialized");
    return ESP_OK;
}

bool ble_hid_host_is_synced(void) {
    return s_synced;
}

esp_err_t ble_hid_host_start_scan(void) {
    if (!s_synced) {
        ESP_LOGW(TAG, "BLE host not synced yet, cannot scan");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_connected) {
        ESP_LOGW(TAG, "Already connected, skipping scan");
        return ESP_ERR_INVALID_STATE;
    }

    /* Cancel any running scan first */
    ble_gap_disc_cancel();

    xSemaphoreTake(s_results_mutex, portMAX_DELAY);
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    xSemaphoreGive(s_results_mutex);

    struct ble_gap_disc_params params = {
        .itvl             = 0,        /* use defaults */
        .window           = 0,
        .filter_policy    = BLE_HCI_SCAN_FILT_NO_WL,
        .limited          = 0,
        .passive          = 0,        /* active scan to get full name */
        .filter_duplicates = 0,
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, SCAN_DURATION_MS, &params,
                          gap_event_cb, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "ble_gap_disc failed: %d", rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BLE scan started (%d ms)", SCAN_DURATION_MS);
    return ESP_OK;
}

void ble_hid_host_stop_scan(void) {
    ble_gap_disc_cancel();
}

void ble_hid_host_clear_results(void) {
    xSemaphoreTake(s_results_mutex, portMAX_DELAY);
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    xSemaphoreGive(s_results_mutex);
}

char *ble_hid_host_get_scan_results_json(void) {
    xSemaphoreTake(s_results_mutex, portMAX_DELAY);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < s_result_count; i++) {
        if (!s_results[i].valid) continue;
        cJSON *obj = cJSON_CreateObject();
        char addr_str[18];
        addr_to_str(&s_results[i].addr, addr_str, sizeof(addr_str));
        cJSON_AddStringToObject(obj, "addr", addr_str);
        cJSON_AddStringToObject(obj, "name", s_results[i].name);
        cJSON_AddNumberToObject(obj, "rssi", s_results[i].rssi);
        cJSON_AddItemToArray(arr, obj);
    }

    xSemaphoreGive(s_results_mutex);

    char *json = cJSON_Print(arr);
    cJSON_Delete(arr);
    return json;
}

esp_err_t ble_hid_host_connect(const char *addr_str) {
    if (s_connected) {
        ESP_LOGW(TAG, "Already connected");
        return ESP_ERR_INVALID_STATE;
    }

    ble_gap_disc_cancel();

    /* Parse "aa:bb:cc:dd:ee:ff" into ble_addr_t (big-endian in string) */
    ble_addr_t addr;
    addr.type = BLE_ADDR_RANDOM;
    unsigned int v[6];
    if (sscanf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &v[5], &v[4], &v[3], &v[2], &v[1], &v[0]) != 6) {
        /* Try public address type if parsing implies it */
        ESP_LOGE(TAG, "Invalid address format: %s", addr_str);
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 6; i++) addr.val[i] = (uint8_t)v[i];

    /* Try to match in scan results to know the correct address type */
    xSemaphoreTake(s_results_mutex, portMAX_DELAY);
    for (int i = 0; i < s_result_count; i++) {
        char a[18];
        addr_to_str(&s_results[i].addr, a, sizeof(a));
        if (strcasecmp(a, addr_str) == 0) {
            addr = s_results[i].addr;
            strlcpy(s_device_name, s_results[i].name, sizeof(s_device_name));
            break;
        }
    }
    xSemaphoreGive(s_results_mutex);

    strlcpy(s_device_addr, addr_str, sizeof(s_device_addr));

    uint8_t own_addr_type = BLE_OWN_ADDR_RANDOM;
    ble_hs_util_ensure_addr(0);

    struct ble_gap_conn_params cp = {
        .itvl_min            = 8,    /* 10 ms */
        .itvl_max            = 16,   /* 20 ms */
        .latency             = 0,
        .supervision_timeout = 500,  /* 5 s */
        .min_ce_len          = 0,
        .max_ce_len          = 0,
    };

    int rc = ble_gap_connect(own_addr_type, &addr, 5000, &cp,
                             gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_connect failed: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connecting to %s...", addr_str);
    return ESP_OK;
}

void ble_hid_host_disconnect(void) {
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

bool ble_hid_host_is_connected(void) {
    return s_connected;
}

const char *ble_hid_host_device_name(void) {
    return s_device_name;
}

const char *ble_hid_host_device_addr(void) {
    return s_device_addr;
}
