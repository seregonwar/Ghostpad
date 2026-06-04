set(common_requires nvs_flash esp_wifi esp_netif lwip esp_http_server esp_event bt mdns)

if(CONFIG_IDF_TARGET_ESP32P4)
    set(target_requires esp_hosted esp_tinyusb)
    set(target_srcs "main.c" "wifi_ap.c" "web_server.c" "hid_gamepad.c" "ble_hid_host.c" "dns_redirect.c")
else()
    set(target_requires bluepad32 btstack)
    set(target_srcs "main.c" "wifi_ap.c" "web_server.c" "hid_gamepad.c" "ghostpad_platform.c" "dns_redirect.c")
endif()

idf_component_register(
    SRCS ${target_srcs}
    INCLUDE_DIRS "."
    REQUIRES ${common_requires} ${target_requires}
)