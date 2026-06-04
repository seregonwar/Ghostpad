#include "hid_gamepad.h"
#include "web_server.h"
#include "esp_log.h"
#include <string.h>

#if CONFIG_IDF_TARGET_ESP32P4
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#endif

static const char *TAG = "ghostpad_hid";

char g_connected_console[32] = "None";

#if CONFIG_IDF_TARGET_ESP32P4

/*
 * =====================================================================================
 *                                CONSTANTS & DESCRIPTORS
 * =====================================================================================
 */

static const tusb_desc_device_t hid_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x054C,
    .idProduct = 0x0CE6,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const tusb_desc_device_qualifier_t hid_desc_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00
};

typedef struct __attribute__((packed)) {
    uint8_t  report_id;
    uint8_t  buttons[4];
    uint8_t  lx;
    uint8_t  ly;
    uint8_t  rx;
    uint8_t  ry;
    uint16_t l2;
    uint16_t r2;
} dualsense_report_t;

static const uint8_t hid_desc_custom[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x10, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x55, 0x00,
    0x65, 0x00, 0x81, 0x02, 0x05, 0x01, 0x09, 0x30,
    0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x04, 0x81,
    0x02, 0x05, 0x01, 0x09, 0x39, 0x09, 0x3a, 0x15,
    0x00, 0x26, 0x03, 0x02, 0x75, 0x10, 0x95, 0x02,
    0xc0,
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(hid_desc_custom), 0x81, 64, 1),
};

static const char *desc_str[] = {
    (const char[]) {0x09, 0x04},
    "Sony Interactive Entertainment",
    "Wireless Controller",
    "Ghostpad Bridge",
};

static void usb_device_event_cb(tinyusb_event_t *event, void *arg) {
    (void)arg;
    if (event->id == TINYUSB_EVENT_ATTACHED) {
        strcpy(g_connected_console, "Detecting...");
    } else if (event->id == TINYUSB_EVENT_DETACHED) {
        strcpy(g_connected_console, "None");
    }
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf) {
    (void)itf;
    return hid_desc_custom;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)itf; (void)report_type; (void)buffer; (void)reqlen;
    if (report_id == 0x05 || report_id == 0x09 || report_id == 0x20) {
        strcpy(g_connected_console, "PS5 Console");
    } else if (report_id == 0x12 || report_id == 0xa3 || report_id == 0xf0 || report_id == 0xf1 || report_id == 0xf2) {
        strcpy(g_connected_console, "PS4 Console");
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    (void)itf; (void)report_type; (void)buffer; (void)bufsize;
    if (report_id == 0x05 || report_id == 0x09 || report_id == 0x20) {
        strcpy(g_connected_console, "PS5 Console");
    } else if (report_id == 0x12 || report_id == 0xa3 || report_id == 0xf0 || report_id == 0xf1 || report_id == 0xf2) {
        strcpy(g_connected_console, "PS4 Console");
    }
}

static uint32_t map_buttons_gpad_to_ps5(uint32_t gpad_buttons) {
    uint32_t ps5 = 0;
    if (gpad_buttons & 0x00000001) ps5 |= 0x00004000;
    if (gpad_buttons & 0x00000002) ps5 |= 0x00008000;
    if (gpad_buttons & 0x00000004) ps5 |= 0x00002000;
    if (gpad_buttons & 0x00000008) ps5 |= 0x00001000;
    if (gpad_buttons & 0x00000010) ps5 |= 0x00000010;
    if (gpad_buttons & 0x00000020) ps5 |= 0x00000020;
    if (gpad_buttons & 0x00000040) ps5 |= 0x00000040;
    if (gpad_buttons & 0x00000080) ps5 |= 0x00000080;
    if (gpad_buttons & 0x00000100) ps5 |= 0x00000400;
    if (gpad_buttons & 0x00000200) ps5 |= 0x00000800;
    if (gpad_buttons & 0x00000400) ps5 |= 0x00000002;
    if (gpad_buttons & 0x00000800) ps5 |= 0x00000004;
    if (gpad_buttons & 0x00001000) ps5 |= 0x00000100;
    if (gpad_buttons & 0x00002000) ps5 |= 0x00000200;
    if (gpad_buttons & 0x00004000) ps5 |= 0x00020000;
    if (gpad_buttons & 0x00008000) ps5 |= 0x00010000;
    if (gpad_buttons & 0x00010000) ps5 |= 0x00000008;
    if (gpad_buttons & 0x00020000) ps5 |= 0x00100000;
    return ps5;
}

#endif /* CONFIG_IDF_TARGET_ESP32P4 */

/*
 * =====================================================================================
 *                               PUBLIC API IMPLEMENTATION
 * =====================================================================================
 */

esp_err_t hid_gamepad_init(void) {
#if CONFIG_IDF_TARGET_ESP32P4
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = &hid_desc_device;
    tusb_cfg.descriptor.qualifier = &hid_desc_qualifier;
    tusb_cfg.descriptor.string = desc_str;
    tusb_cfg.descriptor.string_count = sizeof(desc_str) / sizeof(desc_str[0]);
    tusb_cfg.descriptor.full_speed_config = config_desc;
    tusb_cfg.descriptor.high_speed_config = config_desc;
    tusb_cfg.event_cb = usb_device_event_cb;

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "TinyUSB initialized as DualSense");
#else
    ESP_LOGI(TAG, "USB HID not available on this target (WiFi-only mode)");
#endif
    return ESP_OK;
}

void hid_gamepad_send_report(void) {
#if CONFIG_IDF_TARGET_ESP32P4
    if (!tud_hid_ready()) return;

    dualsense_report_t report;
    memset(&report, 0, sizeof(report));
    report.report_id = 0x01;

    uint32_t ps5_buttons = map_buttons_gpad_to_ps5(g_gamepad.buttons);
    report.buttons[0] = ps5_buttons & 0xff;
    report.buttons[1] = (ps5_buttons >> 8) & 0xff;
    report.buttons[2] = (ps5_buttons >> 16) & 0xff;
    report.buttons[3] = (ps5_buttons >> 24) & 0xff;
    report.lx = g_gamepad.lx;
    report.ly = g_gamepad.ly;
    report.rx = g_gamepad.rx;
    report.ry = g_gamepad.ry;
    report.l2 = (uint16_t)g_gamepad.l2 << 8;
    report.r2 = (uint16_t)g_gamepad.r2 << 8;

    tud_hid_report(0, &report, sizeof(report));
#endif
}
