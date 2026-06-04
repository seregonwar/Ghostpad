#include "wifi_ap.h"
#include "web_server.h"
#include "hid_gamepad.h"
#if CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#include <uni.h>
#include <btstack_port_esp32.h>
struct uni_platform *get_my_platform(void);
#else
#include "ble_hid_host.h"
#endif
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#if CONFIG_ESP_HOSTED_ENABLED
#include "esp_hosted.h"
#endif

static const char *TAG = "ghostpad_main";

#define HID_REPORT_INTERVAL_MS 10

static void hid_sender_task(void *param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        hid_gamepad_send_report();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HID_REPORT_INTERVAL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Ghostpad Bridge starting...");

#if CONFIG_ESP_HOSTED_ENABLED
    ESP_LOGI(TAG, "Initializing ESP32-C6 co-processor over SDIO...");
    ESP_ERROR_CHECK(esp_hosted_init());
    ESP_ERROR_CHECK(esp_hosted_connect_to_slave());
    ESP_LOGI(TAG, "ESP32-C6 co-processor ready");
#endif

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(wifi_ap_init());
    ESP_ERROR_CHECK(web_server_start());
    ESP_ERROR_CHECK(hid_gamepad_init());
#if !CONFIG_BLUEPAD32_PLATFORM_CUSTOM
    ESP_ERROR_CHECK(ble_hid_host_init());
#endif

    xTaskCreate(hid_sender_task, "hid_sender", 2048, NULL, 3, NULL);

    char ip[16] = "";
    if (ghostpad_wifi_get_primary_ip(ip, sizeof(ip)) != ESP_OK) {
        strlcpy(ip, "unknown", sizeof(ip));
    }

    ESP_LOGI(TAG, "Ghostpad Bridge ready");
    ESP_LOGI(TAG, "WiFi mode: %s", ghostpad_wifi_mode_name());
    ESP_LOGI(TAG, "WiFi SSID: %s", ghostpad_wifi_active_ssid());
    ESP_LOGI(TAG, "Web GUI: http://%s", ip);
#if CONFIG_GHOSTPAD_ENABLE_MDNS
    ESP_LOGI(TAG, "Web GUI mDNS: http://%s.local/", CONFIG_GHOSTPAD_WIFI_HOSTNAME);
#endif

#if CONFIG_BLUEPAD32_PLATFORM_CUSTOM
    ESP_LOGI(TAG, "Starting Bluepad32 Custom Platform...");
    btstack_init();
    uni_platform_set_custom(get_my_platform());
    uni_init(0, NULL);
    btstack_run_loop_execute();
#endif
}

