#include "wifi_ap.h"
#include "web_server.h"
#include "hid_gamepad.h"
#include "ble_hid_host.h"
#include "esp_log.h"
#include "esp_hosted.h"
#include "nvs_flash.h"

static const char *TAG = "ghostpad_main";

void app_main(void) {
    ESP_LOGI(TAG, "Ghostpad Bridge starting...");

    ESP_LOGI(TAG, "Initializing ESP32-C6 co-processor over SDIO...");
    ESP_ERROR_CHECK(esp_hosted_init());
    ESP_ERROR_CHECK(esp_hosted_connect_to_slave());
    ESP_LOGI(TAG, "ESP32-C6 co-processor ready");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(wifi_ap_init());
    ESP_ERROR_CHECK(web_server_start());
    ESP_ERROR_CHECK(hid_gamepad_init());
    ESP_ERROR_CHECK(ble_hid_host_init());

    ESP_LOGI(TAG, "Ghostpad Bridge ready");
    ESP_LOGI(TAG, "WiFi AP: %s / %s", WIFI_AP_SSID, WIFI_AP_PASS);
    ESP_LOGI(TAG, "Web GUI: http://192.168.4.1");
}

