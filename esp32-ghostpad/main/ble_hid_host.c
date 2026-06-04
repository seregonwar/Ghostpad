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
#include <strings.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

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
#define BLE_UUID_DEVICE_INFO        0x180A
#define BLE_UUID_BATTERY_SERVICE    0x180F

#define BLE_APPEARANCE_HID_GENERIC  0x03C0
#define BLE_APPEARANCE_HID_JOYSTICK 0x03C3
#define BLE_APPEARANCE_HID_GAMEPAD  0x03C4

#define SONY_USB_VID                0x054C
#define SONY_PID_DS4_CUH_ZCT1       0x05C4
#define SONY_PID_DS4_CUH_ZCT2       0x09CC
#define SONY_PID_DUALSENSE          0x0CE6
#define SONY_PID_DUALSENSE_EDGE     0x0DF2

#define SONY_BLE_COMPANY_ID         0x004C

#define MICROSOFT_USB_VID           0x045E
#define NINTENDO_USB_VID            0x057E
#define EIGHTBITDO_USB_VID          0x2DC8

#define MAX_SCAN_RESULTS            32
#define SCAN_DURATION_MS            12000  /* 12 s active window; pairing advertisements can be sparse */

/*
 * =====================================================================================
 *                               SCAN RESULT STORAGE
 * =====================================================================================
 */

typedef struct {
    ble_addr_t  addr;
    char        name[64];
    char        model[40];
    int8_t      rssi;
    uint16_t    appearance;
    uint8_t     addr_type;
    int         score;
    bool        valid;
    bool        candidate;
    bool        hid_service;
    bool        gamepad_appearance;
    bool        known_name;
    bool        sony_name;
    bool        sony_mfg;
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
static uint16_t s_hid_service_end = 0;

/* True while a GATT discovery is running */
static bool s_discovering = false;

/* True once NimBLE host is synced with the controller */
static bool s_synced = false;
static bool s_scanning = false;
static bool s_auto_connect = false;
static char  s_auto_connect_addr[18] = "";
static int  s_last_scan_rc = 0;
static int  s_last_gap_event = 0;
static int  s_last_disc_count = 0;

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

static int on_disc_descs(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         uint16_t chr_val_handle,
                         const struct ble_gatt_dsc *dsc,
                         void *arg) {
    if (error->status == BLE_HS_EDONE) {
        s_discovering = false;
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Descriptor discovery error: %d", error->status);
        s_discovering = false;
        return 0;
    }

    /* Log each descriptor */
    if (dsc->uuid.u.type == BLE_UUID_TYPE_16) {
        ESP_LOGI(TAG, "  Desc: uuid=0x%04x handle=0x%04x",
                 dsc->uuid.u16.value, dsc->handle);
    }

    /* Look for CCCD (0x2902) among the descriptors of this characteristic */
    if (dsc->uuid.u.type == BLE_UUID_TYPE_16 &&
        dsc->uuid.u16.value == BLE_UUID_CCCD) {
        ESP_LOGI(TAG, "Found CCCD at handle=0x%04x, enabling notifications...",
                 dsc->handle);
        uint8_t val[2] = {0x01, 0x00};
        ble_gattc_write_flat(conn_handle, dsc->handle,
                             val, sizeof(val), on_write_cccd, NULL);
    }
    return 0;
}

static int on_disc_chars(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == BLE_HS_EDONE) {
        if (s_report_handle != 0) {
            /* Discover descriptors of this characteristic to find the CCCD */
            uint16_t desc_end = s_hid_service_end;
            uint16_t desc_start = s_report_handle + 1;
            if (desc_end == 0 || desc_end < desc_start) {
                desc_end = s_report_handle + 20;
            }
            ESP_LOGI(TAG, "Discovering descriptors for HID Report handles=[0x%04x-0x%04x]...",
                     desc_start, desc_end);
            ble_gattc_disc_all_dscs(conn_handle,
                                    desc_start, desc_end,
                                    on_disc_descs, NULL);
        } else {
            ESP_LOGI(TAG, "No notifiable HID Report characteristic found");
            s_discovering = false;
        }
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Characteristic discovery error: %d", error->status);
        s_discovering = false;
        return 0;
    }

    /* Log every characteristic found */
    if (chr->uuid.u.type == BLE_UUID_TYPE_16) {
        ESP_LOGI(TAG, "  Char: uuid=0x%04x props=0x%02x val_handle=0x%04x def_handle=0x%04x",
                 chr->uuid.u16.value, chr->properties,
                 chr->val_handle, chr->def_handle);
    }

    /* Look for the HID Report characteristic with notify property */
    if (chr->uuid.u.type == BLE_UUID_TYPE_16) {
        uint16_t uuid16 = chr->uuid.u16.value;
        if (uuid16 == BLE_UUID_HID_REPORT &&
            (chr->properties & BLE_GATT_CHR_PROP_NOTIFY)) {
            s_report_handle = chr->val_handle;
            ESP_LOGI(TAG, "Found HID Report characteristic, val_handle=0x%04x def_handle=0x%04x",
                     chr->val_handle, chr->def_handle);
        }
    }
    return 0;
}

static int on_disc_service_fallback(uint16_t conn_handle,
                                     const struct ble_gatt_error *error,
                                     const struct ble_gatt_svc *svc, void *arg);

static int on_disc_service(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *svc, void *arg) {
    if (error->status == BLE_HS_EDONE) {
        /* If we didn't find the HID service via disc_svc_by_uuid,
         * try discovering all primary services as fallback. */
        if (s_hid_service_end == 0) {
            ESP_LOGI(TAG, "HID service not found via UUID, trying all services...");
            ble_gattc_disc_all_svcs(conn_handle, on_disc_service_fallback, NULL);
            return 0;
        }
        ESP_LOGI(TAG, "Service discovery complete");
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Service discovery error: %d", error->status);
        return 0;
    }

    ESP_LOGI(TAG, "  Svc: uuid=0x%04x handles=[0x%04x-0x%04x]",
             svc->uuid.u16.value, svc->start_handle, svc->end_handle);

    s_hid_service_end = svc->end_handle;
    ESP_LOGI(TAG, "Found HID Service (0x1812), discovering characteristics...");
    s_discovering = true;
    s_report_handle = 0;
    ble_gattc_disc_all_chrs(conn_handle,
                            svc->start_handle, svc->end_handle,
                            on_disc_chars, NULL);
    return 0;
}

/* Fallback service discovery: look for HID among all primary services,
 * and also check for included services. */
static int on_disc_service_fallback(uint16_t conn_handle,
                                     const struct ble_gatt_error *error,
                                     const struct ble_gatt_svc *svc, void *arg) {
    if (error->status == BLE_HS_EDONE) {
        if (s_hid_service_end == 0) {
            ESP_LOGW(TAG, "HID service (0x1812) not found in any service");
        }
        return 0;
    }
    if (error->status != 0) return 0;

    ESP_LOGI(TAG, "  SvcFallback: uuid=0x%04x handles=[0x%04x-0x%04x]",
             svc->uuid.u16.value, svc->start_handle, svc->end_handle);

    if (svc->uuid.u.type == BLE_UUID_TYPE_16 &&
        svc->uuid.u16.value == BLE_UUID_HID_SERVICE) {
        s_hid_service_end = svc->end_handle;
        ESP_LOGI(TAG, "Found HID Service via fallback, discovering characteristics...");
        s_discovering = true;
        s_report_handle = 0;
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

typedef struct {
    const char *needle;
    const char *model;
    int         score;
    bool        sony;
} controller_name_match_t;

static const controller_name_match_t s_controller_names[] = {
    /* Sony controllers normally appear with one of these names when pairing. */
    {"dualsense edge",              "DualSense Edge",       95, true},
    {"dualsense",                   "DualSense",            90, true},
    {"wireless controller",         "DualShock/DualSense",  80, true},
    {"dualshock",                   "DualShock",            80, true},
    {"dualsense wireless controller","DualSense",           95, true},

    /* Useful generic controller names; these are candidates, not Sony-only. */
    {"xbox wireless controller",    "Xbox Wireless",        70, false},
    {"xbox controller",             "Xbox",                 65, false},
    {"pro controller",              "Nintendo Pro",         65, false},
    {"joy-con",                     "Nintendo Joy-Con",     60, false},
    {"8bitdo",                      "8BitDo",               60, false},
    {"gamepad",                     "Generic Gamepad",      55, false},
    {"joystick",                    "Generic Joystick",     50, false},
};

static bool ascii_contains_ci(const char *haystack, const char *needle) {
    size_t hlen, nlen;
    if (!haystack || !needle) return false;
    hlen = strlen(haystack);
    nlen = strlen(needle);
    if (nlen == 0 || hlen < nlen) return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        if (strncasecmp(haystack + i, needle, nlen) == 0) return true;
    }
    return false;
}

static void classify_controller_adv(const char *name,
                                    uint16_t appearance,
                                    bool hid_service,
                                    bool *candidate,
                                    bool *known_name,
                                    bool *sony_name,
                                    int *score,
                                    char *model,
                                    size_t model_len) {
    int s = 0;
    bool cand = false;
    bool known = false;
    bool sony = false;

    if (model && model_len > 0) model[0] = '\0';

    if (hid_service) {
        s += 50;
        cand = true;
    }
    if (appearance == BLE_APPEARANCE_HID_GAMEPAD ||
        appearance == BLE_APPEARANCE_HID_JOYSTICK ||
        appearance == BLE_APPEARANCE_HID_GENERIC) {
        s += (appearance == BLE_APPEARANCE_HID_GAMEPAD) ? 50 : 35;
        cand = true;
    }

    if (name && name[0]) {
        for (size_t i = 0; i < sizeof(s_controller_names) / sizeof(s_controller_names[0]); i++) {
            if (ascii_contains_ci(name, s_controller_names[i].needle)) {
                if (s_controller_names[i].score > s) s = s_controller_names[i].score;
                known = true;
                cand = true;
                if (s_controller_names[i].sony) sony = true;
                if (model && model_len > 0 && model[0] == '\0') {
                    strlcpy(model, s_controller_names[i].model, model_len);
                }
            }
        }
    }

    if (model && model_len > 0 && model[0] == '\0') {
        if (hid_service || appearance == BLE_APPEARANCE_HID_GAMEPAD) {
            strlcpy(model, "BLE HID Gamepad", model_len);
        } else if (name && name[0]) {
            strlcpy(model, "Named BLE device", model_len);
        } else {
            strlcpy(model, "Unknown BLE device", model_len);
        }
    }

    if (candidate) *candidate = cand;
    if (known_name) *known_name = known;
    if (sony_name) *sony_name = sony;
    if (score) *score = s;
}

static void addr_to_str(const ble_addr_t *addr, char *buf, size_t len) {
    snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr->val[5], addr->val[4], addr->val[3],
             addr->val[2], addr->val[1], addr->val[0]);
}

static const char *addr_type_name(uint8_t type) {
    switch (type) {
    case 0: return "public";
    case 1: return "random";
    case 2: return "public_id";
    case 3: return "random_id";
    default: return "unknown";
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {

    /* ---- Scan result ---- */
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        int rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                         event->disc.length_data);
        if (rc != 0) {
            ESP_LOGW(TAG, "adv parse failed rc=%d len=%d rssi=%d",
                     rc, event->disc.length_data, event->disc.rssi);
            break;
        }

        bool is_hid = false;
        bool is_gamepad = false;
        bool is_candidate = false;
        bool is_known_name = false;
        bool is_sony = false;
        int score = 0;
        uint16_t appearance = fields.appearance_is_present ? fields.appearance : 0;
        char adv_name[64] = "";
        char model[40] = "";

        if (fields.name && fields.name_len > 0) {
            size_t n = fields.name_len < sizeof(adv_name) - 1 ? fields.name_len : sizeof(adv_name) - 1;
            memcpy(adv_name, fields.name, n);
            adv_name[n] = '\0';
        }

        /* Parse manufacturer-specific data: Sony company ID 0x004C identifies
         * DualSense / DualShock 4 even when name/HID/appearance are missing
         * from the advertisement. */
        bool is_sony_mfg = false;
        if (fields.mfg_data && fields.mfg_data_len >= 2) {
            uint16_t company_id = (uint16_t)fields.mfg_data[0]
                                | ((uint16_t)fields.mfg_data[1] << 8);
            if (company_id == SONY_BLE_COMPANY_ID) {
                is_sony_mfg = true;
                ESP_LOGI(TAG, "Sony manufacturer data detected");
            }
        }

        {
            char hexbuf[256];
            int pos = 0;
            for (int i = 0; i < event->disc.length_data && i < 64 && pos < (int)sizeof(hexbuf) - 4; i++) {
                pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02x", event->disc.data[i]);
            }
            hexbuf[pos] = '\0';
            ESP_LOGI(TAG, "RAW ADV rssi=%d len=%d hex=%s name=%s",
                     event->disc.rssi, event->disc.length_data,
                     hexbuf, adv_name[0] ? adv_name : "(none)");
        }

        if (appearance == BLE_APPEARANCE_HID_GAMEPAD ||
            appearance == BLE_APPEARANCE_HID_JOYSTICK ||
            appearance == BLE_APPEARANCE_HID_GENERIC) {
            is_gamepad = true;
        }

        for (int i = 0; i < fields.num_uuids16; i++) {
            if (ble_uuid_u16(&fields.uuids16[i].u) == BLE_UUID_HID_SERVICE) {
                is_hid = true;
                break;
            }
        }

        classify_controller_adv(adv_name, appearance, is_hid,
                                &is_candidate, &is_known_name, &is_sony,
                                &score, model, sizeof(model));

        /* Boost Sony devices identified via manufacturer data even if name
         * and HID UUID are not present in the advertisement. DualSense often
         * omits these from ADV_IND, providing them only after connection. */
        if (is_sony_mfg) {
            is_sony = true;
            is_candidate = true;
            if (score < 85) score = 85;
            strlcpy(model, "DualSense/DualShock", sizeof(model));
        }

        /* Store every device — but flag only true gamepads as candidates.
         * Require BOTH Sony manufacturer data AND either HID service or known
         * gamepad name to avoid false positives from other Sony BLE devices. */
        bool store_candidate = is_sony &&
            (is_hid || is_gamepad || is_known_name || is_sony_mfg);

        xSemaphoreTake(s_results_mutex, portMAX_DELAY);
        bool found = false;
        for (int i = 0; i < s_result_count; i++) {
            if (memcmp(s_results[i].addr.val, event->disc.addr.val, 6) == 0 &&
                s_results[i].addr.type == event->disc.addr.type) {
                found = true;
                /* Update metadata — keep highest score, OR sony flags */
                s_results[i].rssi = event->disc.rssi;
                s_results[i].appearance = appearance;
                s_results[i].addr_type = event->disc.addr.type;
                if (score > s_results[i].score) s_results[i].score = score;
                s_results[i].candidate = s_results[i].candidate || is_candidate;
                s_results[i].hid_service = s_results[i].hid_service || is_hid;
                s_results[i].gamepad_appearance = s_results[i].gamepad_appearance || is_gamepad;
                s_results[i].known_name = s_results[i].known_name || is_known_name;
                s_results[i].sony_name = s_results[i].sony_name || is_sony;
                s_results[i].sony_mfg = s_results[i].sony_mfg || is_sony_mfg;
                if (adv_name[0]) strlcpy(s_results[i].name, adv_name, sizeof(s_results[i].name));
                /* Update model only when we have specific info (not a generic fallback) */
                if (model[0] &&
                    strcmp(model, "Unknown BLE device") != 0 &&
                    strcmp(model, "Named BLE device") != 0) {
                    strlcpy(s_results[i].model, model, sizeof(s_results[i].model));
                }
                break;
            }
        }
        if (!found && s_result_count < MAX_SCAN_RESULTS) {
            scan_result_t *r = &s_results[s_result_count++];
            r->addr  = event->disc.addr;
            r->rssi  = event->disc.rssi;
            r->valid = true;
            r->appearance = appearance;
            r->addr_type = event->disc.addr.type;
            r->score = score;
            r->candidate = is_candidate;
            r->hid_service = is_hid;
            r->gamepad_appearance = is_gamepad;
            r->known_name = is_known_name;
            r->sony_name = is_sony;
            r->sony_mfg = is_sony_mfg;
            if (adv_name[0]) {
                strlcpy(r->name, adv_name, sizeof(r->name));
            } else if (is_sony_mfg) {
                strlcpy(r->name, "Sony Wireless Controller", sizeof(r->name));
            } else if (is_candidate) {
                strlcpy(r->name, "HID Gamepad", sizeof(r->name));
            } else {
                strlcpy(r->name, "BLE Device", sizeof(r->name));
            }
            if (model[0]) strlcpy(r->model, model, sizeof(r->model));
            char addr_str[18];
            addr_to_str(&r->addr, addr_str, sizeof(addr_str));
            ESP_LOGI(TAG, "Discovered: %s (%s) \"%s\" RSSI=%d app=0x%04x hid=%d sony=%d score=%d",
                     addr_str, addr_type_name(r->addr.type), r->name, r->rssi,
                     r->appearance, r->hid_service, r->sony_name, r->score);

            /* Remember first Sony candidate for deferred auto-connect */
            if (s_auto_connect && !s_connected && r->sony_name && r->rssi > -70 &&
                s_auto_connect_addr[0] == '\0') {
                strlcpy(s_auto_connect_addr, addr_str, sizeof(s_auto_connect_addr));
                ESP_LOGI(TAG, "Marked %s for auto-connect (RSSI=%d)", addr_str, r->rssi);
            }
        }
        s_last_disc_count = s_result_count;
        xSemaphoreGive(s_results_mutex);
        break;
    }

    /* ---- Scan complete ---- */
    case BLE_GAP_EVENT_DISC_COMPLETE:
        s_scanning = false;
        s_last_gap_event = event->type;
        ESP_LOGI(TAG, "BLE scan complete (%d devices found)", s_result_count);
        /* Deferred auto-connect: connect to the saved address now that scan is done */
        if (s_auto_connect_addr[0] != '\0' && !s_connected) {
            ESP_LOGI(TAG, "Auto-connecting to %s...", s_auto_connect_addr);
            ble_hid_host_connect(s_auto_connect_addr);
            s_auto_connect_addr[0] = '\0';
        }
        break;

    /* ---- Connected ---- */
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_connected   = true;
            s_auto_connect = false;
            ble_gap_disc_cancel();
            ESP_LOGI(TAG, "Connected! handle=%d  Discovering HID service...",
                     s_conn_handle);
            s_report_handle = 0;
            s_hid_service_end = 0;
            /* Discover HID service: try by UUID first, then fall back to
             * discovering all services and searching for included services. */
            ESP_LOGI(TAG, "Connected! handle=%d  Discovering HID service...",
                     s_conn_handle);
            {
                ble_uuid16_t hid_uuid = BLE_UUID16_INIT(BLE_UUID_HID_SERVICE);
                ble_gattc_disc_svc_by_uuid(s_conn_handle, &hid_uuid.u,
                                           on_disc_service, NULL);
            }
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
        s_hid_service_end = 0;
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

    /* Ensure we have an address to use as scanner. */
    ble_hs_util_ensure_addr(0);
    uint8_t own_addr_type = 0;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    ESP_LOGI(TAG, "BLE own address type: %s rc=%d", addr_type_name(own_addr_type), rc);
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

    ble_gap_disc_cancel();

    xSemaphoreTake(s_results_mutex, portMAX_DELAY);
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    xSemaphoreGive(s_results_mutex);

    uint8_t own_addr_type = 0;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        s_last_scan_rc = rc;
        return ESP_FAIL;
    }

    struct ble_gap_disc_params params = {
        .itvl             = 0x0010,   /* 10 ms */
        .window           = 0x0010,   /* continuous active scan */
        .filter_policy    = BLE_HCI_SCAN_FILT_NO_WL,
        .limited          = 0,
        .passive          = 0,        /* active scan to request scan-response name */
        .filter_duplicates = 0,
    };

    rc = ble_gap_disc(own_addr_type, SCAN_DURATION_MS, &params,
                      gap_event_cb, NULL);
    s_last_scan_rc = rc;
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "ble_gap_disc failed: %d own_addr_type=%s",
                 rc, addr_type_name(own_addr_type));
        return ESP_FAIL;
    }
    s_scanning = true;
    s_auto_connect = true;
    ESP_LOGI(TAG, "BLE scan started (%d ms), own_addr_type=%s",
             SCAN_DURATION_MS, addr_type_name(own_addr_type));
    return ESP_OK;
}

void ble_hid_host_stop_scan(void) {
    ble_gap_disc_cancel();
    s_scanning = false;
    s_auto_connect = false;
}

void ble_hid_host_clear_results(void) {
    xSemaphoreTake(s_results_mutex, portMAX_DELAY);
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    xSemaphoreGive(s_results_mutex);
}


char *ble_hid_host_get_debug_json(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddBoolToObject(root, "synced", s_synced);
    cJSON_AddBoolToObject(root, "scanning", s_scanning);
    cJSON_AddBoolToObject(root, "connected", s_connected);
    cJSON_AddNumberToObject(root, "last_scan_rc", s_last_scan_rc);
    cJSON_AddNumberToObject(root, "last_gap_event", s_last_gap_event);
    cJSON_AddNumberToObject(root, "result_count", s_last_disc_count);
    cJSON_AddStringToObject(root, "device_name", s_device_name);
    cJSON_AddStringToObject(root, "device_addr", s_device_addr);
    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    return json;
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
        cJSON_AddNumberToObject(obj, "addr_type", s_results[i].addr_type);
        cJSON_AddStringToObject(obj, "name", s_results[i].name);
        cJSON_AddStringToObject(obj, "model", s_results[i].model);
        cJSON_AddNumberToObject(obj, "rssi", s_results[i].rssi);
        cJSON_AddNumberToObject(obj, "score", s_results[i].score);
        cJSON_AddNumberToObject(obj, "appearance", s_results[i].appearance);
        cJSON_AddBoolToObject(obj, "candidate", s_results[i].candidate);
        cJSON_AddBoolToObject(obj, "hid_service", s_results[i].hid_service);
        cJSON_AddBoolToObject(obj, "gamepad_appearance", s_results[i].gamepad_appearance);
        cJSON_AddBoolToObject(obj, "known_name", s_results[i].known_name);
        cJSON_AddBoolToObject(obj, "sony_name", s_results[i].sony_name);
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

    /* Parse "aa:bb:cc:dd:ee:ff" into ble_addr_t (big-endian in string) */
    ble_addr_t addr;
    addr.type = BLE_ADDR_RANDOM;
    unsigned int v[6];
    if (sscanf(addr_str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &v[5], &v[4], &v[3], &v[2], &v[1], &v[0]) != 6) {
        ESP_LOGE(TAG, "Invalid address format: %s", addr_str);
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 6; i++) addr.val[i] = (uint8_t)v[i];

    /* Try to match in scan results to know the correct address type */
    bool found_addr = false;
    xSemaphoreTake(s_results_mutex, portMAX_DELAY);
    for (int i = 0; i < s_result_count; i++) {
        char a[18];
        addr_to_str(&s_results[i].addr, a, sizeof(a));
        if (strcasecmp(a, addr_str) == 0) {
            addr = s_results[i].addr;
            strlcpy(s_device_name, s_results[i].name, sizeof(s_device_name));
            found_addr = true;
            break;
        }
    }
    xSemaphoreGive(s_results_mutex);

    if (!found_addr) {
        ESP_LOGW(TAG, "Address %s not found in scan cache; trying with random type", addr_str);
    }

    strlcpy(s_device_addr, addr_str, sizeof(s_device_addr));

    uint8_t own_addr_type = 0;
    int own_rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (own_rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed before connect: %d", own_rc);
        return ESP_FAIL;
    }

    /* Pass NULL for conn_params to use NimBLE defaults (more compatible) */
    ESP_LOGI(TAG, "Connecting to %s (type=%s, own=%s)...",
             addr_str, addr_type_name(addr.type), addr_type_name(own_addr_type));
    int rc = ble_gap_connect(own_addr_type, &addr, 8000, NULL,
                             gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_connect failed: %d (addr_type=%s own_addr_type=%s)",
                 rc, addr_type_name(addr.type), addr_type_name(own_addr_type));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ble_gap_connect initiated to %s", addr_str);
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
