/* SPDX-License-Identifier: GPL-3.0-or-later
 * klog.c — /dev/klog reader, MBus event parser, TCP bridge, callbacks
 */

#include "klog.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#else
#define klog_printf(...) fprintf(stderr, __VA_ARGS__)
#endif

/* Declared in main.c — log sink for persistent status file */
extern void ghostpad_status_log(const char *fmt, ...);
#define KP_LOG(fmt, ...) ghostpad_status_log("[KLOG] " fmt, ##__VA_ARGS__)

/* ── Utilities ────────────────────────────────────────────────────────── */

uint64_t klog_parse_hex(const char *str) {
    uint64_t val = 0;
    while (*str) {
        char c = *str++;
        if      (c >= '0' && c <= '9') val = (val << 4) | (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val = (val << 4) | (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = (val << 4) | (uint64_t)(c - 'A' + 10);
        else break;
    }
    return val;
}

const char *klog_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t nl = strlen(needle);
    if (nl == 0) return haystack;
    for (; *haystack; haystack++) {
        size_t i;
        for (i = 0; i < nl; i++) {
            char h = haystack[i], n = needle[i];
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) break;
        }
        if (i == nl) return haystack;
    }
    return NULL;
}

/* ── Backlog ring buffer ──────────────────────────────────────────────── */

static pthread_mutex_t g_bl_lock = PTHREAD_MUTEX_INITIALIZER;
static char   g_bl_buf[KLOG_BACKLOG_SIZE];
static size_t g_bl_head = 0, g_bl_len = 0;
static uint64_t g_bl_total = 0;

static void backlog_append(const char *buf, size_t len) {
    if (!buf || len == 0) return;
    pthread_mutex_lock(&g_bl_lock);
    if (len >= sizeof(g_bl_buf)) {
        memcpy(g_bl_buf, buf + len - sizeof(g_bl_buf), sizeof(g_bl_buf));
        g_bl_head = 0; g_bl_len = sizeof(g_bl_buf);
    } else {
        size_t first = sizeof(g_bl_buf) - g_bl_head;
        if (first > len) first = len;
        memcpy(g_bl_buf + g_bl_head, buf, first);
        if (len > first) memcpy(g_bl_buf, buf + first, len - first);
        g_bl_head = (g_bl_head + len) % sizeof(g_bl_buf);
        g_bl_len += len; if (g_bl_len > sizeof(g_bl_buf)) g_bl_len = sizeof(g_bl_buf);
    }
    g_bl_total += len;
    pthread_mutex_unlock(&g_bl_lock);
}

static int backlog_replay(int fd) {
    char *snapshot = NULL; size_t len = 0, start = 0;
    pthread_mutex_lock(&g_bl_lock);
    len = g_bl_len;
    if (len > 0) {
        snapshot = (char *)malloc(len);
        if (snapshot) {
            start = (g_bl_head + sizeof(g_bl_buf) - len) % sizeof(g_bl_buf);
            size_t first = sizeof(g_bl_buf) - start;
            if (first > len) first = len;
            memcpy(snapshot, g_bl_buf + start, first);
            if (len > first) memcpy(snapshot + first, g_bl_buf, len - first);
        }
    }
    pthread_mutex_unlock(&g_bl_lock);
    if (len == 0 || !snapshot) return snapshot ? 0 : -1;
    size_t w = 0;
    while (w < len) { ssize_t n = write(fd, snapshot + w, len - w); if (n < 0) break; w += (size_t)n; }
    free(snapshot);
    return 0;
}

/* ── TCP bridge ───────────────────────────────────────────────────────── */

static pthread_mutex_t g_cl_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_clients[KLOG_MAX_CLIENTS] = { -1, -1, -1, -1 };

static void bridge_broadcast(const char *buf, size_t len) {
    if (!buf || len == 0) return;
    pthread_mutex_lock(&g_cl_lock);
    for (int i = 0; i < KLOG_MAX_CLIENTS; i++) {
        int fd = g_clients[i];
        if (fd < 0) continue;
        size_t w = 0; int ok = 1;
        while (w < len) { ssize_t n = write(fd, buf + w, len - w); if (n <= 0) { ok = 0; break; } w += (size_t)n; }
        if (!ok) { close(fd); g_clients[i] = -1; }
    }
    pthread_mutex_unlock(&g_cl_lock);
}

static void bridge_add(int fd) {
    backlog_replay(fd);
    pthread_mutex_lock(&g_cl_lock);
    for (int i = 0; i < KLOG_MAX_CLIENTS; i++)
        if (g_clients[i] < 0) { g_clients[i] = fd; pthread_mutex_unlock(&g_cl_lock); return; }
    pthread_mutex_unlock(&g_cl_lock);
    close(fd);
}

static void *bridge_thread(void *arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in sin; memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET; sin.sin_addr.s_addr = htonl(INADDR_ANY); sin.sin_port = htons(KLOG_PORT);
    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0 || listen(sock, 5) < 0) { close(sock); return NULL; }
    KP_LOG("bridge listening on port %d\n", KLOG_PORT);
    while (1) {
        fd_set set; FD_ZERO(&set); FD_SET(sock, &set);
        struct timeval tv = { 1, 0 };
        if (select(sock + 1, &set, NULL, NULL, &tv) <= 0) continue;
        if (FD_ISSET(sock, &set)) {
            int c = accept(sock, NULL, NULL);
            if (c >= 0) {
#ifdef SO_NOSIGPIPE
                int o2 = 1; setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &o2, sizeof(o2));
#endif
                bridge_add(c);
            }
        }
    }
    close(sock); return NULL;
}

/* ── Device ID callbacks ──────────────────────────────────────────────── */

#define KLOG_MAX_CBS 8
static klog_device_cb g_cbs[KLOG_MAX_CBS];
static void *g_cb_ctx[KLOG_MAX_CBS];
static int g_cb_count = 0;
static pthread_mutex_t g_cb_lock = PTHREAD_MUTEX_INITIALIZER;

/* Convenience queue for klog_dequeue_id() */
#define KLOG_Q_SIZE 16
static uint64_t g_id_q[KLOG_Q_SIZE];
static int g_id_qw = 0, g_id_qr = 0;
static pthread_mutex_t g_id_q_lock = PTHREAD_MUTEX_INITIALIZER;

static void dispatch_device_id(uint64_t id) {
    if (!id) return;
    /* Enqueue for blocking dequeue consumers */
    pthread_mutex_lock(&g_id_q_lock);
    int next = (g_id_qw + 1) % KLOG_Q_SIZE;
    if (next != g_id_qr) { g_id_q[g_id_qw] = id; g_id_qw = next; }
    pthread_mutex_unlock(&g_id_q_lock);
    /* Dispatch to registered callbacks */
    pthread_mutex_lock(&g_cb_lock);
    for (int i = 0; i < g_cb_count; i++) g_cbs[i](id, g_cb_ctx[i]);
    pthread_mutex_unlock(&g_cb_lock);
}

void klog_on_device(klog_device_cb cb, void *ctx) {
    pthread_mutex_lock(&g_cb_lock);
    if (g_cb_count < KLOG_MAX_CBS) { g_cbs[g_cb_count] = cb; g_cb_ctx[g_cb_count] = ctx; g_cb_count++; }
    pthread_mutex_unlock(&g_cb_lock);
}

uint64_t klog_dequeue_id(int timeout_ms) {
    for (int t = 0; t < timeout_ms; t += 100) {
        pthread_mutex_lock(&g_id_q_lock);
        if (g_id_qw != g_id_qr) { uint64_t id = g_id_q[g_id_qr]; g_id_qr = (g_id_qr + 1) % KLOG_Q_SIZE; pthread_mutex_unlock(&g_id_q_lock); return id; }
        pthread_mutex_unlock(&g_id_q_lock);
        usleep(100000);
    }
    return 0;
}

/* ── MBus event parser ────────────────────────────────────────────────── */

static uint64_t parse_dev_id(const char *line) {
    const char *keys[] = { "DeviceId", "DeviceID", "deviceId", "deviceID", "device id" };
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        const char *p = klog_strcasestr(line, keys[i]);
        if (!p) continue;
        p += strlen(keys[i]);
        while (*p == ' ' || *p == '\t' || *p == '=' || *p == ':') p++;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        return klog_parse_hex(p);
    }
    return 0;
}

static void parse_line(const char *line) {
    if (!klog_strcasestr(line, "DEVICE_ADDED") &&
        !klog_strcasestr(line, "DeviceAdded") &&
        !klog_strcasestr(line, "GetUnassignedDeviceInfo"))
        return;

    uint64_t dev_id = parse_dev_id(line);
    if (!dev_id) return;

    int is_phys = klog_strcasestr(line, "capabilityBattery:1") != NULL;
    int is_unassigned =
        klog_strcasestr(line, "capabilityBattery:0") != NULL ||
        klog_strcasestr(line, "userId=0xffffffff") != NULL ||
        klog_strcasestr(line, "UserId:0xffffffff") != NULL ||
        klog_strcasestr(line, "UserId:0x000000ff") != NULL ||
        klog_strcasestr(line, "UserId:0xff") != NULL;

    int is_remoteplay =
        klog_strcasestr(line, "REMOTEPLAY") != NULL ||
        ((klog_strcasestr(line, "type:4") || klog_strcasestr(line, "type=4")) &&
         (klog_strcasestr(line, "subType:2") || klog_strcasestr(line, "subtype:2") ||
          klog_strcasestr(line, "subType=2") || klog_strcasestr(line, "subtype=2")));

    int is_ps5_vda =
        (klog_strcasestr(line, "type:1") || klog_strcasestr(line, "type=1")) &&
        (klog_strcasestr(line, "subType:22") || klog_strcasestr(line, "subtype:22") ||
         klog_strcasestr(line, "subType=22") || klog_strcasestr(line, "subtype=22")) &&
        klog_strcasestr(line, "capabilityBattery:0") != NULL;

    if (is_phys) {
        KP_LOG("physical controller 0x%llx\n", (unsigned long long)dev_id);
        return;
    }

    if (is_remoteplay || is_ps5_vda || is_unassigned) {
        KP_LOG("VDA candidate 0x%llx%s%s%s\n",
               (unsigned long long)dev_id,
               is_remoteplay ? " remoteplay" : "",
               is_ps5_vda ? " ps5-vda" : "",
               is_unassigned ? " unassigned" : "");
        dispatch_device_id(dev_id);
    }
}

/* ── /dev/klog reader thread ──────────────────────────────────────────── */

static void *reader_thread(void *arg) {
    (void)arg;
    char buf[512], line[1024]; size_t ll = 0;
    int fd = -1;

    /* /dev/klog may be EBUSY if another payload left it open.
     * Retry for up to 30s — the previous reader usually releases it. */
    for (int retry = 0; retry < 150 && fd < 0; retry++) {
        fd = open("/dev/klog", O_RDONLY);
        if (fd < 0 && errno == EBUSY) { usleep(200000); continue; }
        break;
    }
    if (fd < 0) { KP_LOG("open /dev/klog failed errno=%d\n", errno); return NULL; }
    KP_LOG("capture started, backlog=%u\n", (unsigned)sizeof(g_bl_buf));
    while (1) {
        ssize_t len = read(fd, buf, sizeof(buf));
        if (len < 0) { if (errno == EINTR) continue; usleep(200000); continue; }
        if (len == 0) { usleep(10000); continue; }
        backlog_append(buf, (size_t)len);
        bridge_broadcast(buf, (size_t)len);
        for (ssize_t i = 0; i < len; i++) {
            char c = buf[i];
            if (c == '\n' || ll >= sizeof(line) - 1) { line[ll] = '\0'; parse_line(line); ll = 0; }
            else if (c != '\r') line[ll++] = c;
        }
    }
    close(fd); return NULL;
}

/* ── Init ─────────────────────────────────────────────────────────────── */

int klog_init(void) {
    pthread_t rt, bt;
    if (pthread_create(&rt, NULL, reader_thread, NULL) == 0) pthread_detach(rt);
    else { KP_LOG("reader thread failed\n"); return -1; }
    if (pthread_create(&bt, NULL, bridge_thread, NULL) == 0) pthread_detach(bt);
    else KP_LOG("bridge thread failed (non-fatal)\n");
    return 0;
}
