#include "web_server.h"
#include "web_gui.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "hid_gamepad.h"
#include "ble_hid_host.h"
#include <string.h>
#include <stdlib.h>
#include "esp_wifi_ap_get_sta_list.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

static const char *TAG = "ghostpad_web";

typedef struct __attribute__((packed)) {
    char     magic[4];   /* Must be "GPAD" */
    uint32_t buttons;    /* Button bitmask */
    uint8_t  lx;         /* Left stick X  (128=center) */
    uint8_t  ly;         /* Left stick Y  (128=center) */
    uint8_t  rx;         /* Right stick X (128=center) */
    uint8_t  ry;         /* Right stick Y (128=center) */
    uint8_t  l2;         /* L2 analog (0-255) */
    uint8_t  r2;         /* R2 analog (0-255) */
    uint8_t  _pad[2];    /* Reserved, must be 0 */
} GhostpadPacket;

static int console_tcp_socket = -1;
static char current_console_ip[16] = "";

/* Payload storage (uploaded via web UI) */
static uint8_t *s_payload_data = NULL;
static size_t  s_payload_size   = 0;
static char     s_payload_name[64] = "";

static int connect_to_console(const char *ip, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    struct timeval timeout = { .tv_sec = 0, .tv_usec = timeout_ms * 1000 };
    if (select(sock + 1, NULL, &fdset, NULL, &timeout) == 1) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
            fcntl(sock, F_SETFL, flags);
            return sock;
        }
    }

    close(sock);
    return -1;
}
volatile gamepad_state_t g_gamepad = {0};

static char local_ip[16] = "192.168.4.1";
static uint32_t uptime_sec = 0;

static void reset_gamepad(void) {
    g_gamepad.buttons = 0;
    g_gamepad.lx = 128;
    g_gamepad.ly = 128;
    g_gamepad.rx = 128;
    g_gamepad.ry = 128;
    g_gamepad.l2 = 0;
    g_gamepad.r2 = 0;
}

static int check_port_open(const char *ip, int port, int timeout_ms) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(sock);
        return 0;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    struct timeval timeout = { .tv_sec = 0, .tv_usec = timeout_ms * 1000 };
    if (select(sock + 1, NULL, &fdset, NULL, &timeout) == 1) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        close(sock);
        return (so_error == 0) ? 1 : 0;
    }

    close(sock);
    return 0;
}

static void scan_connected_stations(cJSON *devices) {
    wifi_sta_list_t wifi_sta_list;
    wifi_sta_mac_ip_list_t netif_sta_list;

    if (esp_wifi_ap_get_sta_list(&wifi_sta_list) == ESP_OK) {
        if (esp_wifi_ap_get_sta_list_with_ip(&wifi_sta_list, &netif_sta_list) == ESP_OK) {
            ESP_LOGI(TAG, "SoftAP has %d connected clients. Scanning ports...", netif_sta_list.num);
            for (int i = 0; i < netif_sta_list.num; i++) {
                char ip_str[16];
                esp_ip4_addr_t ip = netif_sta_list.sta[i].ip;
                if (ip.addr == 0) continue;
                inet_ntoa_r(ip, ip_str, sizeof(ip_str));
                ESP_LOGI(TAG, "Scanning client IP: %s", ip_str);

                int is_elf  = check_port_open(ip_str, 9021, 50);
                int is_klog = check_port_open(ip_str, 3232, 50);
                int is_9090 = check_port_open(ip_str, 9090, 50);
                int is_gpad = check_port_open(ip_str, 6967, 50);
                int is_ctrl = check_port_open(ip_str, 6970, 50);

                if (is_elf || is_klog || is_9090 || is_gpad || is_ctrl) {
                    cJSON *dev = cJSON_CreateObject();
                    cJSON_AddStringToObject(dev, "name", "PlayStation (Wi-Fi)");
                    cJSON_AddStringToObject(dev, "icon", "🎮");

                    char desc[96];
                    snprintf(desc, sizeof(desc), "%s  |  ELF:%s  KLOG:%s  9090:%s  GPAD:%s  CTRL:%s",
                             ip_str,
                             is_elf ? "✓" : "✗",
                             is_klog ? "✓" : "✗",
                             is_9090 ? "✓" : "✗",
                             is_gpad ? "✓" : "✗",
                             is_ctrl ? "✓" : "✗");
                    cJSON_AddStringToObject(dev, "desc", desc);
                    cJSON_AddStringToObject(dev, "ip", ip_str);
                    cJSON_AddStringToObject(dev, "status", (is_gpad || is_ctrl) ? "online" : "pairing");
                    cJSON_AddItemToArray(devices, dev);
                }
            }
        }
    }
}

static void scan_subnet(cJSON *devices) {
    char base[16];
    strlcpy(base, local_ip, sizeof(base));

    char *dot = strrchr(base, '.');
    if (!dot) return;
    *dot = '\0';

    for (int i = 1; i < 255; i++) {
        char ip[20];
        snprintf(ip, sizeof(ip), "%s.%d", base, i);

        int is_elf  = check_port_open(ip, 9021, 50);
        int is_klog = check_port_open(ip, 3232, 50);
        int is_9090 = check_port_open(ip, 9090, 50);
        int is_gpad = check_port_open(ip, 6967, 50);
        int is_ctrl = check_port_open(ip, 6970, 50);

        if (is_elf || is_klog || is_9090 || is_gpad || is_ctrl) {
            cJSON *dev = cJSON_CreateObject();
            cJSON_AddStringToObject(dev, "name", "PlayStation (Wi-Fi)");
            cJSON_AddStringToObject(dev, "icon", "🎮");

            char desc[96];
            snprintf(desc, sizeof(desc), "%s  |  ELF:%s  KLOG:%s  9090:%s  GPAD:%s  CTRL:%s",
                     ip,
                     is_elf ? "✓" : "✗",
                     is_klog ? "✓" : "✗",
                     is_9090 ? "✓" : "✗",
                     is_gpad ? "✓" : "✗",
                     is_ctrl ? "✓" : "✗");
            cJSON_AddStringToObject(dev, "desc", desc);
            cJSON_AddStringToObject(dev, "ip", ip);
            cJSON_AddStringToObject(dev, "status", (is_gpad || is_ctrl) ? "online" : "pairing");
            cJSON_AddItemToArray(devices, dev);
        }
    }
}

static esp_err_t scan_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *devices = cJSON_AddArrayToObject(root, "devices");

    cJSON *usb = cJSON_CreateObject();
    cJSON_AddStringToObject(usb, "name", "PS5 (USB)");
    cJSON_AddStringToObject(usb, "icon", "🔗");
    cJSON_AddStringToObject(usb, "desc", "Connected via USB HID");
    cJSON_AddStringToObject(usb, "ip", "usb:0");
    cJSON_AddStringToObject(usb, "status", "online");
    cJSON_AddItemToArray(devices, usb);

    scan_connected_stations(devices);

    // Fall back to subnet scan only if not already connected to a console
    if (cJSON_GetArraySize(devices) <= 1 && console_tcp_socket < 0) {
        scan_subnet(devices);
    }

    const char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "wifi_ssid", "Ghostpad-ESP32");
    cJSON_AddStringToObject(root, "ip", local_ip);

    wifi_sta_list_t sta_list;
    esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
    cJSON_AddNumberToObject(root, "clients", (err == ESP_OK) ? sta_list.num : 0);

    uptime_sec++;
    cJSON_AddNumberToObject(root, "uptime", uptime_sec);

    cJSON *usb = cJSON_CreateObject();
    cJSON_AddStringToObject(usb, "device", g_connected_console);
    cJSON_AddBoolToObject(usb, "connected", strcmp(g_connected_console, "None") != 0);
    cJSON_AddItemToObject(root, "usb", usb);

    cJSON *wifi_console = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_console, "ip", current_console_ip);
    cJSON_AddBoolToObject(wifi_console, "connected", console_tcp_socket >= 0);
    cJSON_AddItemToObject(root, "wifi_console", wifi_console);

    const char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

static esp_err_t captive_404_handler(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "Redirecting to Ghostpad");
    return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL,
};

static const httpd_uri_t scan_uri = {
    .uri       = "/api/scan",
    .method    = HTTP_GET,
    .handler   = scan_handler,
    .user_ctx  = NULL,
};

static const httpd_uri_t status_uri = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL,
};

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "New WebSocket handshake, resetting wireless console socket.");
        if (console_tcp_socket >= 0) {
            close(console_tcp_socket);
            console_tcp_socket = -1;
            strcpy(current_console_ip, "");
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_frame = {0};
    ws_frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t err = httpd_ws_recv_frame(req, &ws_frame, 0);
    if (err != ESP_OK) return ESP_OK;
    if (ws_frame.len == 0 || ws_frame.len > 512) return ESP_OK;

    char *buf = calloc(1, ws_frame.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    ws_frame.payload = (uint8_t *)buf;

    err = httpd_ws_recv_frame(req, &ws_frame, ws_frame.len);
    if (err != ESP_OK) {
        free(buf);
        return ESP_OK;
    }
    buf[ws_frame.len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) return ESP_OK;

    cJSON *target_ip = cJSON_GetObjectItem(json, "target_ip");
    if (cJSON_IsString(target_ip)) {
        if (console_tcp_socket >= 0) {
            close(console_tcp_socket);
            console_tcp_socket = -1;
            strcpy(current_console_ip, "");
        }
        if (strcmp(target_ip->valuestring, "usb") != 0 && strlen(target_ip->valuestring) > 0) {
            ESP_LOGI(TAG, "Connecting to wireless console at %s...", target_ip->valuestring);
            console_tcp_socket = connect_to_console(target_ip->valuestring, 6967, 1000);
            if (console_tcp_socket >= 0) {
                ESP_LOGI(TAG, "Connected to wireless console at %s!", target_ip->valuestring);
                strlcpy(current_console_ip, target_ip->valuestring, sizeof(current_console_ip));
            } else {
                ESP_LOGE(TAG, "Failed to connect to wireless console at %s", target_ip->valuestring);
            }
        }
    }

    cJSON *buttons = cJSON_GetObjectItem(json, "buttons");
    cJSON *lx = cJSON_GetObjectItem(json, "lx");
    cJSON *ly = cJSON_GetObjectItem(json, "ly");
    cJSON *rx = cJSON_GetObjectItem(json, "rx");
    cJSON *ry = cJSON_GetObjectItem(json, "ry");
    cJSON *l2 = cJSON_GetObjectItem(json, "l2");
    cJSON *r2 = cJSON_GetObjectItem(json, "r2");

    if (cJSON_IsNumber(buttons)) g_gamepad.buttons = (uint32_t)buttons->valueint;
    if (cJSON_IsNumber(lx)) g_gamepad.lx = (uint8_t)lx->valueint;
    if (cJSON_IsNumber(ly)) g_gamepad.ly = (uint8_t)ly->valueint;
    if (cJSON_IsNumber(rx)) g_gamepad.rx = (uint8_t)rx->valueint;
    if (cJSON_IsNumber(ry)) g_gamepad.ry = (uint8_t)ry->valueint;
    if (cJSON_IsNumber(l2)) g_gamepad.l2 = (uint8_t)l2->valueint;
    if (cJSON_IsNumber(r2)) g_gamepad.r2 = (uint8_t)r2->valueint;

    cJSON_Delete(json);

    if (console_tcp_socket >= 0) {
        GhostpadPacket pkt;
        memcpy(pkt.magic, "GPAD", 4);
        pkt.buttons = htonl(g_gamepad.buttons);
        pkt.lx = g_gamepad.lx;
        pkt.ly = g_gamepad.ly;
        pkt.rx = g_gamepad.rx;
        pkt.ry = g_gamepad.ry;
        pkt.l2 = g_gamepad.l2;
        pkt.r2 = g_gamepad.r2;
        pkt._pad[0] = 0;
        pkt._pad[1] = 0;

        int sent_bytes = send(console_tcp_socket, &pkt, sizeof(pkt), 0);
        if (sent_bytes < 0) {
            ESP_LOGE(TAG, "Failed to send packet to wireless console, closing socket");
            close(console_tcp_socket);
            console_tcp_socket = -1;
            strcpy(current_console_ip, "");
        }
    } else {
        hid_gamepad_send_report();
    }

    httpd_ws_frame_t ws_resp = {0};
    ws_resp.type = HTTPD_WS_TYPE_TEXT;
    ws_resp.payload = (uint8_t *)"ok";
    ws_resp.len = 2;
    httpd_ws_send_frame(req, &ws_resp);

    return ESP_OK;
}

static const httpd_uri_t ws_uri = {
    .uri       = "/ws",
    .method    = HTTP_GET,
    .handler   = ws_handler,
    .user_ctx  = NULL,
    .is_websocket = true,
};

/*
 * =====================================================================================
 *                            BLE API ENDPOINTS
 *
 *  GET  /api/ble/scan       — start scan, return discovered device list
 *  GET  /api/ble/status     — BLE connection state
 *  POST /api/ble/connect    — body: {"addr":"xx:xx:xx:xx:xx:xx"}
 *  POST /api/ble/disconnect — disconnect current controller
 * =====================================================================================
 */

static esp_err_t ble_scan_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();

    /* Check if NimBLE host is synced */
    if (!ble_hid_host_is_synced()) {
        cJSON_AddBoolToObject(root, "synced", false);
        cJSON_AddStringToObject(root, "error", "BLE host not synced yet, retry in a few seconds");
        cJSON_AddBoolToObject(root, "connected", false);
        cJSON_AddStringToObject(root, "device_name", ble_hid_host_device_name());
        cJSON_AddStringToObject(root, "device_addr", ble_hid_host_device_addr());

        const char *json = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        free((void *)json);
        cJSON_Delete(root);
        return ESP_OK;
    }

    /* Kick off a new scan */
    esp_err_t scan_err = ble_hid_host_start_scan();

    cJSON_AddBoolToObject(root, "synced", true);

    if (scan_err != ESP_OK) {
        cJSON_AddBoolToObject(root, "scan_ok", false);
        cJSON_AddStringToObject(root, "error", "Failed to start BLE scan");
        cJSON_AddBoolToObject(root, "connected", ble_hid_host_is_connected());
        cJSON_AddStringToObject(root, "device_name", ble_hid_host_device_name());
        cJSON_AddStringToObject(root, "device_addr", ble_hid_host_device_addr());

        const char *json = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        free((void *)json);
        cJSON_Delete(root);
        return ESP_OK;
    }

    /* Short busy-wait so the first few advertisements are caught */
    vTaskDelay(pdMS_TO_TICKS(2000));
    ble_hid_host_stop_scan();

    char *results_json = ble_hid_host_get_scan_results_json();

    cJSON_AddBoolToObject(root, "scan_ok", true);
    cJSON_AddRawToObject(root, "devices", results_json ? results_json : "[]");
    cJSON_AddBoolToObject(root, "connected", ble_hid_host_is_connected());
    cJSON_AddStringToObject(root, "device_name", ble_hid_host_device_name());
    cJSON_AddStringToObject(root, "device_addr", ble_hid_host_device_addr());

    const char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    free(results_json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ble_status_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "connected",   ble_hid_host_is_connected());
    cJSON_AddStringToObject(root, "device_name", ble_hid_host_device_name());
    cJSON_AddStringToObject(root, "device_addr", ble_hid_host_device_addr());

    const char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ble_connect_handler(httpd_req_t *req) {
    size_t len = req->content_len;
    if (len == 0 || len > 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
        return ESP_OK;
    }

    char *buf = calloc(1, len + 1);
    if (!buf) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM"); return ESP_OK; }

    if (httpd_req_recv(req, buf, len) <= 0) {
        free(buf);
        return ESP_OK;
    }
    buf[len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *addr_item = cJSON_GetObjectItem(json, "addr");
    if (!cJSON_IsString(addr_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing addr");
        return ESP_OK;
    }

    esp_err_t ret = ble_hid_host_connect(addr_item->valuestring);
    cJSON_Delete(json);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", ret == ESP_OK);
    const char *resp_str = cJSON_Print(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp_str);
    free((void *)resp_str);
    cJSON_Delete(resp);
    return ESP_OK;
}

static esp_err_t ble_disconnect_handler(httpd_req_t *req) {
    ble_hid_host_disconnect();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static const httpd_uri_t ble_scan_uri = {
    .uri     = "/api/ble/scan",
    .method  = HTTP_GET,
    .handler = ble_scan_handler,
};

static const httpd_uri_t ble_status_uri = {
    .uri     = "/api/ble/status",
    .method  = HTTP_GET,
    .handler = ble_status_handler,
};

static const httpd_uri_t ble_connect_uri = {
    .uri     = "/api/ble/connect",
    .method  = HTTP_POST,
    .handler = ble_connect_handler,
};

static const httpd_uri_t ble_disconnect_uri = {
    .uri     = "/api/ble/disconnect",
    .method  = HTTP_POST,
    .handler = ble_disconnect_handler,
};

/*
 * =====================================================================================
 *                          PAYLOAD UPLOAD / DEPLOY API
 *
 *  GET  /api/payload         — payload status (loaded / size / name)
 *  POST /api/payload         — upload payload binary (body = raw .elf)
 *  POST /api/deploy          — send payload to target console
 *                              body: {"ip":"192.168.x.x", "port":9021}
 * =====================================================================================
 */

static void free_payload(void) {
    if (s_payload_data) {
        free(s_payload_data);
        s_payload_data = NULL;
    }
    s_payload_size = 0;
    s_payload_name[0] = '\0';
}

static esp_err_t payload_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "loaded", s_payload_data != NULL && s_payload_size > 0);
    cJSON_AddNumberToObject(root, "size", s_payload_size);
    cJSON_AddStringToObject(root, "name", s_payload_name);

    const char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t payload_post_handler(httpd_req_t *req) {
    size_t len = req->content_len;
    if (len == 0 || len > 512 * 1024) { /* max 512KB */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid size (max 512KB)");
        return ESP_OK;
    }

    uint8_t *buf = malloc(len);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }

    int received = 0;
    while (received < len) {
        int r = httpd_req_recv(req, (char *)buf + received, len - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Read error");
            return ESP_OK;
        }
        received += r;
    }

    free_payload();
    s_payload_data = buf;
    s_payload_size = len;

    /* Try to extract filename from Content-Disposition header */
    char cd_buf[256] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Disposition", cd_buf, sizeof(cd_buf)) == ESP_OK) {
        const char *fn = strstr(cd_buf, "filename=");
        if (fn) {
            fn += 9;
            if (*fn == '"') fn++;
            int i = 0;
            while (*fn && *fn != '"' && *fn != ';' && i < (int)sizeof(s_payload_name) - 1)
                s_payload_name[i++] = *fn++;
            s_payload_name[i] = '\0';
        }
    }
    if (s_payload_name[0] == '\0')
        strlcpy(s_payload_name, "payload.elf", sizeof(s_payload_name));

    ESP_LOGI(TAG, "Payload uploaded: %s (%d bytes)", s_payload_name, len);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t deploy_handler(httpd_req_t *req) {
    if (!s_payload_data || s_payload_size == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No payload loaded");
        return ESP_OK;
    }

    size_t len = req->content_len;
    if (len == 0 || len > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
        return ESP_OK;
    }

    char *buf = calloc(1, len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }

    if (httpd_req_recv(req, buf, len) <= 0) {
        free(buf);
        return ESP_OK;
    }
    buf[len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *ip_item   = cJSON_GetObjectItem(json, "ip");
    cJSON *port_item = cJSON_GetObjectItem(json, "port");

    if (!cJSON_IsString(ip_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ip");
        return ESP_OK;
    }

    int port = 9021;
    if (cJSON_IsNumber(port_item)) {
        port = port_item->valueint;
    }

    const char *target_ip = ip_item->valuestring;
    ESP_LOGI(TAG, "Deploying payload to %s:%d (%d bytes)", target_ip, port, s_payload_size);

    int sock = connect_to_console(target_ip, port, 5000);
    if (sock < 0) {
        cJSON_Delete(json);
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", false);
        cJSON_AddStringToObject(resp, "error", "Connection failed");
        const char *r = cJSON_Print(resp);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, r);
        free((void *)r);
        cJSON_Delete(resp);
        return ESP_OK;
    }

    size_t sent = 0;
    bool deploy_ok = true;
    while (sent < s_payload_size) {
        int n = send(sock, s_payload_data + sent, s_payload_size - sent, 0);
        if (n <= 0) {
            ESP_LOGE(TAG, "Deploy send failed at %d/%d", sent, s_payload_size);
            deploy_ok = false;
            break;
        }
        sent += n;
    }

    close(sock);

    ESP_LOGI(TAG, "Deploy %s: sent %d/%d bytes",
             deploy_ok ? "OK" : "FAILED", sent, s_payload_size);

    cJSON_Delete(json);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", deploy_ok);
    if (!deploy_ok) {
        cJSON_AddStringToObject(resp, "error", "Send failed");
    }
    cJSON_AddNumberToObject(resp, "sent", sent);
    cJSON_AddNumberToObject(resp, "total", s_payload_size);

    const char *r = cJSON_Print(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, r);
    free((void *)r);
    cJSON_Delete(resp);
    return ESP_OK;
}

static const httpd_uri_t payload_get_uri = {
    .uri     = "/api/payload",
    .method  = HTTP_GET,
    .handler = payload_get_handler,
};

static const httpd_uri_t payload_post_uri = {
    .uri     = "/api/payload",
    .method  = HTTP_POST,
    .handler = payload_post_handler,
};

static const httpd_uri_t deploy_uri = {
    .uri     = "/api/deploy",
    .method  = HTTP_POST,
    .handler = deploy_handler,
};

/*
 * =====================================================================================
 *                              KLOG WEBSOCKET
 * =====================================================================================
 */

/* State for the klog TCP reader (shared between WS handler and reader task) */
static int s_klog_tcp_fd = -1;
static TaskHandle_t s_klog_task = NULL;
static httpd_handle_t s_klog_hd = NULL;
static int s_klog_client_fd = -1;
static bool s_klog_running = false;

/* Work item queued to HTTPD worker to send data over the WS */
struct klog_work_msg {
    httpd_handle_t hd;
    int fd;
    size_t len;
    uint8_t data[];
};

static void klog_work_cb(void *arg) {
    struct klog_work_msg *m = arg;
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = m->data,
        .len = m->len
    };
    esp_err_t err = httpd_ws_send_frame_async(m->hd, m->fd, &frame);
    ESP_LOGI(TAG, "klog sent %zu bytes via WS: %d", m->len, err);
    free(m);
}

static void klog_reader_task(void *arg) {
    uint8_t buf[512];
    ESP_LOGI(TAG, "klog reader task started (fd=%d)", s_klog_tcp_fd);
    while (s_klog_running && s_klog_tcp_fd >= 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(s_klog_tcp_fd, &fds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
        int s = select(s_klog_tcp_fd + 1, &fds, NULL, NULL, &tv);
        if (s < 0) break;
        if (s == 0) continue;
        int n = read(s_klog_tcp_fd, buf, sizeof(buf));
        if (n <= 0) break;
        ESP_LOGI(TAG, "klog read %d bytes from TCP", n);
        struct klog_work_msg *m = malloc(sizeof(*m) + n);
        if (!m) continue;
        m->hd  = s_klog_hd;
        m->fd  = s_klog_client_fd;
        m->len = n;
        memcpy(m->data, buf, n);
        httpd_queue_work(s_klog_hd, klog_work_cb, m);
    }
    ESP_LOGI(TAG, "klog reader task exiting");
    close(s_klog_tcp_fd);
    s_klog_tcp_fd = -1;
    s_klog_running = false;
    vTaskDelete(NULL);
}

static void klog_stop(void) {
    s_klog_running = false;
    if (s_klog_tcp_fd >= 0) {
        close(s_klog_tcp_fd);
        s_klog_tcp_fd = -1;
    }
}

static esp_err_t klog_ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "klog WebSocket handshake");
        s_klog_hd = req->handle;
        s_klog_client_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }

    httpd_ws_frame_t ws = {0};
    ws.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws, 0);
    if (ret != ESP_OK || ws.len == 0 || ws.len > 512) {
        ESP_LOGI(TAG, "klog WS recv error or closed");
        klog_stop();
        return ESP_OK;
    }

    char *buf = calloc(1, ws.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;
    ws.payload = (uint8_t *)buf;
    ret = httpd_ws_recv_frame(req, &ws, ws.len);
    if (ret != ESP_OK) {
        free(buf);
        klog_stop();
        return ESP_OK;
    }

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) return ESP_OK;
    cJSON *cmd = cJSON_GetObjectItem(json, "cmd");

    if (cmd && cJSON_IsString(cmd)) {
        if (strcmp(cmd->valuestring, "connect") == 0) {
            cJSON *ip = cJSON_GetObjectItem(json, "ip");
            if (ip && cJSON_IsString(ip) && strlen(ip->valuestring) > 0) {
                klog_stop();
                ESP_LOGI(TAG, "klog connecting to %s:3232", ip->valuestring);
                int sock = connect_to_console(ip->valuestring, 3232, 3000);
                if (sock < 0) {
                    ESP_LOGE(TAG, "klog connection failed");
                    httpd_ws_frame_t err = {
                        .type = HTTPD_WS_TYPE_TEXT,
                        .payload = (uint8_t *)"{\"event\":\"error\",\"msg\":\"Connection failed\"}",
                        .len = 37
                    };
                    httpd_ws_send_frame(req, &err);
                } else {
                    ESP_LOGI(TAG, "klog connected to %s:3232", ip->valuestring);
                    s_klog_tcp_fd = sock;
                    s_klog_hd = req->handle;
                    s_klog_client_fd = httpd_req_to_sockfd(req);
                    s_klog_running = true;
                    xTaskCreate(klog_reader_task, "klog_rd", 4096, NULL, 5, &s_klog_task);
                    httpd_ws_frame_t ok = {
                        .type = HTTPD_WS_TYPE_TEXT,
                        .payload = (uint8_t *)"{\"event\":\"connected\"}",
                        .len = 20
                    };
                    httpd_ws_send_frame(req, &ok);
                }
            }
        } else if (strcmp(cmd->valuestring, "disconnect") == 0) {
            klog_stop();
        }
    }
    cJSON_Delete(json);
    return ESP_OK;
}

static const httpd_uri_t klog_ws_uri = {
    .uri     = "/ws/klog",
    .method  = HTTP_GET,
    .handler = klog_ws_handler,
    .is_websocket = true,
};

/*
 * =====================================================================================
 *                              SERVER STARTUP
 * =====================================================================================
 */

esp_err_t web_server_start(void) {
    reset_gamepad();

    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        inet_ntop(AF_INET, &ip_info.ip, local_ip, sizeof(local_ip));
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &scan_uri);
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &ws_uri);
    httpd_register_uri_handler(server, &ble_scan_uri);
    httpd_register_uri_handler(server, &ble_status_uri);
    httpd_register_uri_handler(server, &ble_connect_uri);
    httpd_register_uri_handler(server, &ble_disconnect_uri);
    httpd_register_uri_handler(server, &payload_get_uri);
    httpd_register_uri_handler(server, &payload_post_uri);
    httpd_register_uri_handler(server, &deploy_uri);
    httpd_register_uri_handler(server, &klog_ws_uri);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_404_handler);

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}
