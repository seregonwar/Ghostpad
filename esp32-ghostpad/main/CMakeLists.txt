idf_component_register(
    SRCS "main.c" "wifi_ap.c" "web_server.c" "hid_gamepad.c" "ble_hid_host.c" "dns_redirect.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif lwip esp_http_server esp_event esp_hosted esp_tinyusb bt mdns
)
