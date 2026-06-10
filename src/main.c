/* SPDX-License-Identifier: GPL-3.0-or-later
 * Ghostpad - PS5 Remote Controller Payload
 *
 * Listens on TCP port 6967 for GhostpadPackets sent from the Python GUI,
 * then injects them as virtual DualSense controller input using the
 * scePadVirtualDevice API.
 *
 * Build with: make
 * Deploy with: make test PS5_HOST=<your ps5 ip>
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

/* BSD/POSIX socket headers (available on FreeBSD-based PS5) */
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
#include <ps5/klog.h>
#include <ps5/mdbg.h>
#endif

#ifdef __ORBIS__
#include <ps4/kernel.h>
#include <ps4/klog.h>
#include <ps4/mdbg.h>
#include <dlfcn.h>
#endif

#include "shellui_pad.h"
#include "controller.h"
#include "klog.h"

#ifndef GHOSTPAD_ENABLE_USB_CONTROLLERS
#define GHOSTPAD_ENABLE_USB_CONTROLLERS 1
#endif

#if GHOSTPAD_ENABLE_USB_CONTROLLERS
#include "ctrl_manager.h"
#endif

#ifndef GHOSTPAD_ENABLE_INTERNAL_KLOG
#define GHOSTPAD_ENABLE_INTERNAL_KLOG 1
#endif

/*
 * Safety defaults for the PS4/Orbis bring-up path.
 *
 * The previous build enabled three invasive mechanisms at startup:
 *   - an in-payload /dev/klog TCP bridge with auto-bind side effects,
 *   - direct SceShellCore VDA memory patching,
 *   - a scePadSetProcessPrivilege detour used as a fake VDA trigger.
 *
 * Runtime VDA patching remains opt-in, but the patcher is now manifest-verified
 * for the PS4 libScePad fingerprint captured by vda_probe.
 */
#ifndef GHOSTPAD_ENABLE_INTERNAL_KLOG
#define GHOSTPAD_ENABLE_INTERNAL_KLOG 0
#endif

#ifndef GHOSTPAD_ENABLE_STARTUP_VDA_PATCH
#define GHOSTPAD_ENABLE_STARTUP_VDA_PATCH 0
#endif

#ifndef GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH
#define GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH 1
#endif

#ifndef GHOSTPAD_ENABLE_SETPRIV_HOOK
#define GHOSTPAD_ENABLE_SETPRIV_HOOK 0
#endif

#ifndef GHOSTPAD_ENABLE_ORBIS_SETPRIV_VDA_SENTINEL
#define GHOSTPAD_ENABLE_ORBIS_SETPRIV_VDA_SENTINEL 0
#endif

/* Persistent status log. This is intentionally separate from /dev/klog so
 * failures in the klog reader/bridge do not hide critical startup state. */
#ifndef GHOSTPAD_STATUS_LOG_DIR
#define GHOSTPAD_STATUS_LOG_DIR  "/data/ghostpad"
#endif

#ifndef GHOSTPAD_STATUS_LOG_PATH
#define GHOSTPAD_STATUS_LOG_PATH "/data/ghostpad/ghostpad_status.log"
#endif

#define LOG_MSG_MAX 480

static pthread_mutex_t g_status_log_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_status_log_fd = -1;

static int ghostpad_status_open_locked(int truncate) {
    int flags = O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND);

    (void)mkdir(GHOSTPAD_STATUS_LOG_DIR, 0755);

    if (g_status_log_fd >= 0) {
        close(g_status_log_fd);
        g_status_log_fd = -1;
    }

    g_status_log_fd = open(GHOSTPAD_STATUS_LOG_PATH, flags, 0600);
    return g_status_log_fd;
}

void ghostpad_status_log_reset(void) {
    pthread_mutex_lock(&g_status_log_lock);
    if (ghostpad_status_open_locked(1) >= 0) {
        static const char header[] =
            "===== Ghostpad status log start =====\n"
            "path=/data/ghostpad/ghostpad_status.log\n";
        (void)write(g_status_log_fd, header, sizeof(header) - 1);
    }
    pthread_mutex_unlock(&g_status_log_lock);
}

void ghostpad_status_log(const char *fmt, ...) {
    char buf[LOG_MSG_MAX];
    va_list ap;
    int n;

    if (!fmt) {
        return;
    }

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return;
    }
    buf[sizeof(buf) - 1] = '\0';

    /* Keep existing klog behavior for users who can read /dev/klog. */
    klog_printf("%s", buf);

    pthread_mutex_lock(&g_status_log_lock);
    if (g_status_log_fd < 0) {
        (void)ghostpad_status_open_locked(0);
    }
    if (g_status_log_fd >= 0) {
        size_t len = strnlen(buf, sizeof(buf));
        if (len > 0) {
            (void)write(g_status_log_fd, buf, len);
            if (buf[len - 1] != '\n') {
                (void)write(g_status_log_fd, "\n", 1);
            }
        }
    }
    pthread_mutex_unlock(&g_status_log_lock);
}

/* gp_log — log to klog and to /data/ghostpad/ghostpad_status.log. */
static void gp_log(const char *fmt, ...) {
    char buf[LOG_MSG_MAX];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[LOG_MSG_MAX - 1] = '\0';
    ghostpad_status_log("%s", buf);
}

/* SCE function declarations — linked via sce_stubs (-lSceUserService -lScePad) */

/* User Service */
extern int32_t sceUserServiceInitialize(void *params);
extern int32_t sceUserServiceTerminate(void);
extern int32_t sceUserServiceGetInitialUser(int32_t *outUserId);
extern int32_t sceUserServiceGetForegroundUser(int32_t *outUserId); /* preferred */

/* Pad Library */
extern int32_t scePadInit(void);
extern int32_t scePadOpen(int32_t userId, int32_t type, int32_t index, void *param);
extern int32_t scePadClose(int32_t handle);
extern int32_t scePadGetHandle(int32_t userId, int32_t type, int32_t index);
extern int32_t scePadSetProcessPrivilege(int32_t privilege);
extern int32_t scePadVirtualDeviceAddDevice(void *param, int32_t deviceType);
/* scePadVertualDeviceAddDevice (typo variant) removed — blocks the IPC daemon indefinitely */
extern int32_t scePadVirtualDeviceDeleteDevice(int32_t handle);
extern int32_t scePadVirtualDeviceInsertData(int32_t handle, const void *padData);

/* Kernel Notification (toast message on PS5 UI) */
extern int32_t sceKernelSendNotificationRequest(int unk0, void *req, size_t size, int unk1);


/* Virtual device type: 3 is the working VDA/RemotePlay path on PS4 and
 * DualSense path on PS5.  Type 0 is a classic DS4 pad type, but on PS4
 * scePadVirtualDeviceAddDevice returns 0x80920001 for it, so do not use it
 * as the default VDA type. */
#define VIRTUAL_DEVICE_TYPE_DUALSENSE  3
#define VIRTUAL_DEVICE_TYPE_DS4COMPAT  0

/* PS5/Prospero VDA differs from PS4/Orbis:
 *  - AddDevice must be attempted with userId=1, type=3, size=32.
 *  - scePadVirtualDeviceAddDevice may return 0x803b0006 while still creating
 *    an MBus DEVICE_ADDED event; do not treat that as terminal failure.
 *  - The resulting MBus event appears as type=1/subType=22/capabilityBattery:0.
 */
#ifdef __PROSPERO__
#define GHOSTPAD_PLATFORM_PS5 1
#else
#define GHOSTPAD_PLATFORM_PS5 0
#endif
#define GHOSTPAD_PS5_VDA_COMPAT_RET ((int32_t)0x803b0006)

static int32_t ghostpad_vda_add_user_id(int32_t direct_user_id) {
    return GHOSTPAD_PLATFORM_PS5 ? 1 : direct_user_id;
}

static int ghostpad_vda_ret_can_create_mbus_device(int32_t ret) {
    return ret >= 0 || (GHOSTPAD_PLATFORM_PS5 && ret == GHOSTPAD_PS5_VDA_COMPAT_RET);
}

/* Network packet protocol (shared with Python GUI, ~60 Hz over TCP) */
#define GP_MAGIC      "GPAD"
#define GP_MAGIC_LEN  4
#define GP_PORT       6967
#define GP_CTRL_PORT  6970   /* GBND/HVDI diagnostic control port */

typedef struct __attribute__((packed)) {
    char     magic[4];   /* Must be "GPAD" */
    uint32_t buttons;    /* Button bitmask (same bit layout as SCE_PAD_BUTTON_*) */
    uint8_t  lx;         /* Left stick X  (128=center) */
    uint8_t  ly;         /* Left stick Y  (128=center) */
    uint8_t  rx;         /* Right stick X (128=center) */
    uint8_t  ry;         /* Right stick Y (128=center) */
    uint8_t  l2;         /* L2 analog (0-255) */
    uint8_t  r2;         /* R2 analog (0-255) */
    uint8_t  _pad[2];    /* Reserved, must be 0 */
} GhostpadPacket;

/* Total packet size: 16 bytes */
#define GP_PACKET_SIZE sizeof(GhostpadPacket)

/* GBND: bind command.
 * physicalDevId: currently-connected physical DualSense to evict (0 = skip evict). */
typedef struct __attribute__((packed)) {
    char     magic[4];
    uint32_t userId;
    uint64_t virtualDevId;
    uint64_t physicalDevId;  /* device to sceMbusDisconnect before/after bind */
} GhostpadBindPacket;
#define GP_BIND_PACKET_SIZE sizeof(GhostpadBindPacket)

/* HVDI: handle+VDI test — Python sends the CIM padHandle extracted from klog.
 * Payload calls pt_call VDI(handle, cross_data) in SceShellUI to confirm input. */
typedef struct __attribute__((packed)) {
    char     magic[4];    /* "HVDI" */
    uint32_t padHandle;   /* system padHandle from CIM line */
} GhostpadHvdiPacket;
#define GP_HVDI_PACKET_SIZE sizeof(GhostpadHvdiPacket)

typedef struct __attribute__((packed)) {
    char     magic[4];    /* "TYPE" */
    uint32_t deviceType;  /* libScePad virtual device type */
} GhostpadTypePacket;
#define GP_TYPE_PACKET_SIZE sizeof(GhostpadTypePacket)

/* Notification helper — shows PS5 toast notification.
 * Also used by ctrl_manager.c — keep non-static. */
void ghostpad_notify(const char *fmt, ...) {
    struct { char _unk[45]; char message[3075]; } req;
    va_list ap;
    memset(&req, 0, sizeof(req));
    va_start(ap, fmt);
    vsnprintf(req.message, sizeof(req.message), fmt, ap);
    va_end(ap);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

/* recv_exact — receive exactly n bytes, handling short reads */
static int recv_exact(int fd, void *buf, size_t n) {
    size_t received = 0;
    while (received < n) {
        ssize_t r = recv(fd, (char *)buf + received, n - received, 0);
        if (r <= 0) return (int)r;
        received += (size_t)r;
    }
    return (int)received;
}

static void set_recv_timeout_ms(int fd, int timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

/* Credential elevation — patch ucred authid/caps so scePadInit and VDI succeed.
 * Tries game authid (0x3800000000010003) first; falls back to ptrace authid. */
static void elevate_credentials(void) {
    pid_t mypid = getpid();
    uint8_t privcaps[16] = {
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
    };
    int ret;

    /* Try game-process authid (0x3800000000010003) first — game processes can
     * call scePadOpen successfully.  If that still gives IPMI errors, fall back
     * to the ptrace authid (0x4800000000010003).  The pad IPMI service checks
     * the caller's authid against an allow-list; game authid should be on it. */
    ret = kernel_set_ucred_authid(mypid, 0x3800000000010003l);
    gp_log("[Ghostpad] set_ucred_authid(game): %d\n", ret);
    ret = kernel_set_ucred_caps(mypid, privcaps);
    gp_log("[Ghostpad] set_ucred_caps: %d\n", ret);

    ret = scePadInit();
    gp_log("[Ghostpad] scePadInit(game-authid): 0x%08x\n", ret);

    if (ret < 0) {
        /* Fall back to ptrace authid */
        ret = kernel_set_ucred_authid(mypid, 0x4800000000010003l);
        gp_log("[Ghostpad] set_ucred_authid(ptrace): %d\n", ret);
        ret = scePadInit();
        gp_log("[Ghostpad] scePadInit(ptrace-authid): 0x%08x\n", ret);
    }
}

/* Server-side press rate measurement — logs server_cps every 50 presses via klog */
typedef struct {
    uint64_t      presses;       /* packets with buttons != 0 */
    uint64_t      releases;      /* packets with buttons == 0 */
    uint64_t      vdi_ok;        /* VDI calls returning 0     */
    uint64_t      vdi_err;       /* VDI calls returning error */
    uint64_t      vdi_latency_us_total; /* sum of VDI call durations */
    uint64_t      vdi_latency_samples;
    struct timeval start_tv;
    int           started;
} PressStats;

static PressStats g_stats;

static void stats_reset(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

static void stats_record(int is_press, int vdi_ret, uint64_t vdi_us) {
    if (is_press) {
        if (!g_stats.started) {
            gettimeofday(&g_stats.start_tv, NULL);
            g_stats.started = 1;
        }
        g_stats.presses++;
        if (vdi_ret == 0) g_stats.vdi_ok++;
        else              g_stats.vdi_err++;
    } else {
        g_stats.releases++;
    }

    if (vdi_us > 0) {
        g_stats.vdi_latency_us_total += vdi_us;
        g_stats.vdi_latency_samples++;
    }

    /* Log every 50 presses so klog shows server_cps frequently */
    if (is_press && g_stats.presses % 50 == 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        double elapsed = (double)(now.tv_sec  - g_stats.start_tv.tv_sec) +
                         (double)(now.tv_usec - g_stats.start_tv.tv_usec) * 1e-6;
        double cps = (elapsed > 0.0) ? ((double)g_stats.presses / elapsed) : 0.0;
        uint64_t avg_us = (g_stats.vdi_latency_samples > 0)
            ? (g_stats.vdi_latency_us_total / g_stats.vdi_latency_samples) : 0;
        gp_log("[Ghostpad] server_cps: presses=%llu ok=%llu err=%llu "
               "elapsed=%.2fs cps=%.1f vdi_avg_us=%llu\n",
               (unsigned long long)g_stats.presses,
               (unsigned long long)g_stats.vdi_ok,
               (unsigned long long)g_stats.vdi_err,
               elapsed, cps,
               (unsigned long long)avg_us);
    }
}



static int32_t  userId           = -1;
static int32_t  padHandle        = -1;
static pid_t    shellui_pid      = -1;
static intptr_t shellui_args     = 0;
static int32_t  vdi_handle       = -1;
static uint64_t bound_virtual_device_id = 0;
#ifdef __ORBIS__
static int32_t  virtual_device_type = VIRTUAL_DEVICE_TYPE_DUALSENSE;
#else
static int32_t  virtual_device_type = VIRTUAL_DEVICE_TYPE_DUALSENSE;
#endif
static int      device_vdi_ready = 0;
static int32_t  injectUserId     = -1;

static uint64_t g_last_bound_virt_id = 0;

static pthread_mutex_t g_klog_candidate_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_klog_candidate_vdev_id = 0;
static uint64_t g_klog_candidate_seq = 0;
static uint64_t g_klog_candidate_consumed_seq = 0;

typedef int32_t (*GhostpadMbusBindDeviceWithUserIdFn)(uint64_t deviceId, int32_t userId);
static GhostpadMbusBindDeviceWithUserIdFn g_mbus_bind_device_with_user_id = NULL;

/* ── VDA candidate tracking (Ghostpad-specific binding logic) ──────── */

static void ghostpad_vda_callback(uint64_t dev_id, void *ctx) {
    (void)ctx;
    if (dev_id == 0) return;

    pthread_mutex_lock(&g_klog_candidate_lock);
    if (g_klog_candidate_vdev_id != dev_id) {
        g_klog_candidate_vdev_id = dev_id;
        g_klog_candidate_seq++;
        gp_log("[Ghostpad] klog observed VDA candidate deviceId=0x%llx seq=%llu\n",
               (unsigned long long)dev_id,
               (unsigned long long)g_klog_candidate_seq);
    }
    pthread_mutex_unlock(&g_klog_candidate_lock);
}

static int klog_take_vda_candidate(uint64_t *dev_id_out) {
    int found = 0;
    if (dev_id_out) *dev_id_out = 0;
    pthread_mutex_lock(&g_klog_candidate_lock);
    if (g_klog_candidate_vdev_id != 0 &&
        g_klog_candidate_seq != g_klog_candidate_consumed_seq) {
        if (dev_id_out) *dev_id_out = g_klog_candidate_vdev_id;
        g_klog_candidate_consumed_seq = g_klog_candidate_seq;
        found = 1;
    }
    pthread_mutex_unlock(&g_klog_candidate_lock);
    return found;
}

static int ghostpad_try_klog_candidate_bind(int32_t default_user)
{
    uint64_t vDevId = 0;
    if (!klog_take_vda_candidate(&vDevId) || vDevId == 0) {
        return 0;
    }

    int32_t bindUid = (default_user != 0) ? default_user : injectUserId;
    if (bindUid < 0) {
        bindUid = userId;
    }
    if (bindUid < 0) {
        bindUid = 0x10000000;
    }

    int32_t dev_h = (int32_t)(vDevId & 0xffffffffu);
    gp_log("[Ghostpad] klog prebind candidate: virt=0x%llx user=0x%x handle=0x%x\n",
           (unsigned long long)vDevId, (uint32_t)bindUid, (uint32_t)dev_h);

    int br = -1;
    if (g_mbus_bind_device_with_user_id) {
        br = g_mbus_bind_device_with_user_id(vDevId, bindUid);
        gp_log("[Ghostpad] klog prebind: direct sceMbusBindDeviceWithUserId ret=0x%x\n", (uint32_t)br);
    } else {
        br = shellui_pad_force_bind(vDevId, bindUid);
        gp_log("[Ghostpad] klog prebind: shellui_pad_force_bind ret=0x%x\n", (uint32_t)br);
    }

    if (br != 0) {
        return 0;
    }

    ScePadData probe;
    gp_pad_neutral(&probe);

    int vr = scePadVirtualDeviceInsertData(dev_h, &probe);
    gp_log("[Ghostpad] klog prebind: direct InsertData devId=0x%x ret=0x%x\n",
           (uint32_t)dev_h, (uint32_t)vr);

    if (vr == 0) {
        padHandle = dev_h;
        vdi_handle = dev_h;
        device_vdi_ready = 1;
        bound_virtual_device_id = vDevId;
        g_last_bound_virt_id = vDevId;
        gp_log("[Ghostpad] *** KLOG PREBIND DIRECT VDI ACTIVE: padHandle=0x%x ***\n",
               (uint32_t)padHandle);
        ghostpad_notify("Ghostpad: VDI active 0x%x", (uint32_t)padHandle);
        return 1;
    }

    return 0;
}

static int ghostpad_ctrl_try_bind_once(int ctrlFd, int32_t default_user)
{
    int cfd;
    char magic[4];

    if (ctrlFd < 0) {
        return 0;
    }

    cfd = accept(ctrlFd, NULL, NULL);
    if (cfd < 0) {
        return 0;
    }

    set_recv_timeout_ms(cfd, 750);
    int nr4 = recv_exact(cfd, magic, 4);
    if (nr4 != 4) {
        gp_log("[Ghostpad] ctrlFd(prebind): short magic recv nr4=%d errno=%d\n", nr4, errno);
        close(cfd);
        return 0;
    }

    if (memcmp(magic, "TYPE", 4) == 0) {
        GhostpadTypePacket tpkt;
        memcpy(tpkt.magic, magic, 4);
        if (recv_exact(cfd, (char *)&tpkt + 4, GP_TYPE_PACKET_SIZE - 4) ==
            (int)(GP_TYPE_PACKET_SIZE - 4)) {
            virtual_device_type = (int32_t)tpkt.deviceType;
            gp_log("[Ghostpad] TYPE(prebind): virtual_device_type=%d\n", virtual_device_type);
        }
        close(cfd);
        return 0;
    }

    if (memcmp(magic, "GBND", 4) != 0) {
        gp_log("[Ghostpad] ctrlFd(prebind): ignored magic '%c%c%c%c'\n",
               magic[0], magic[1], magic[2], magic[3]);
        close(cfd);
        return 0;
    }

    GhostpadBindPacket bpkt;
    memcpy(bpkt.magic, magic, 4);
    int nr = recv_exact(cfd, (char *)&bpkt + 4, GP_BIND_PACKET_SIZE - 4);
    close(cfd);
    if (nr != (int)(GP_BIND_PACKET_SIZE - 4)) {
        gp_log("[Ghostpad] GBND(prebind): short packet nr=%d errno=%d\n", nr, errno);
        return 0;
    }

    uint64_t vDevId  = bpkt.virtualDevId;
    uint64_t pDevId  = bpkt.physicalDevId;
    int32_t bindUid  = (bpkt.userId != 0) ? (int32_t)bpkt.userId : default_user;
    int32_t dev_h    = (int32_t)(vDevId & 0xffffffffu);

    gp_log("[Ghostpad] GBND(prebind): virt=0x%llx phys=0x%llx user=0x%x handle=0x%x\n",
           (unsigned long long)vDevId,
           (unsigned long long)pDevId,
           (uint32_t)bindUid,
           (uint32_t)dev_h);

    bound_virtual_device_id = vDevId;

    if (pDevId != 0) {
        int dr = shellui_pad_disconnect_device(pDevId);
        gp_log("[Ghostpad] GBND(prebind): disconnect_device ret=%d\n", dr);
        usleep(200000);
    }

    int br = shellui_pad_force_bind(vDevId, bindUid);
    gp_log("[Ghostpad] GBND(prebind): force_bind ret=%d\n", br);

    if (br != 0) {
        return 0;
    }

    int tr = shellcore_pad_test_vdi_neutral(dev_h);
    gp_log("[Ghostpad] GBND(prebind): shellcore vdi_neutral ret=%d\n", tr);

    ScePadData probe;
    gp_pad_neutral(&probe);

    int vr = scePadVirtualDeviceInsertData(dev_h, &probe);
    gp_log("[Ghostpad] GBND(prebind): direct InsertData devId=0x%x ret=0x%x\n",
           (uint32_t)dev_h, (uint32_t)vr);

    if (tr == 0 || vr == 0) {
        vdi_handle = dev_h;
        device_vdi_ready = 1;
        padHandle = dev_h;
        gp_log("[Ghostpad] *** GBND prebind active: padHandle=0x%x ***\n",
               (uint32_t)padHandle);
        ghostpad_notify("Ghostpad: GBND VDI active 0x%x", (uint32_t)padHandle);
        return 1;
    }

    gp_log("[Ghostpad] GBND(prebind): bind ok but VDI test failed; waiting for another GBND/HVDI\n");
    return 0;
}

/* ---- Main ---- */
int main(void) {
    int      serverFd         = -1;
    int      ctrlFd           = -1;
    int      clientFd         = -1;
    int      ret;
    int      vda_pending_mbus_bind = 0;

    ghostpad_status_log_reset();
    gp_log("[Ghostpad] persistent status log: %s\n", GHOSTPAD_STATUS_LOG_PATH);

    /* Notify immediately so we know the payload is running at all.
     * If this notification never appears, the ELF didn't load. */
    ghostpad_notify("Ghostpad: by StonedModder and SeregonWar");

#ifdef __ORBIS__
    /* Load libSceMbus to prevent PRX_NOT_RESOLVED_FUNCTION crash when calling libScePad functions */
    void *mbus_handle = dlopen("/system/common/lib/libSceMbus.sprx", RTLD_NOW | RTLD_GLOBAL);
    gp_log("[Ghostpad] dlopen(libSceMbus): %p\n", mbus_handle);
    if (mbus_handle) {
        g_mbus_bind_device_with_user_id =
            (GhostpadMbusBindDeviceWithUserIdFn)dlsym(mbus_handle, "sceMbusBindDeviceWithUserId");
        gp_log("[Ghostpad] dlsym(libSceMbus): bind=%p\n",
               (void *)g_mbus_bind_device_with_user_id);
    }
#endif

    gp_log("[Ghostpad] ===== Ghostpad v1.0 Starting =====\n");
    gp_log("[Ghostpad] Control port: %d\n", GP_PORT);
    gp_log("[Ghostpad] build config: internal_klog=%d klog_port=%d runtime_vda_patch=%d startup_vda_patch=%d\n",
           GHOSTPAD_ENABLE_INTERNAL_KLOG, KLOG_PORT,
           GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH, GHOSTPAD_ENABLE_STARTUP_VDA_PATCH);

    /* ---- Initialize SCE User Service ---- */
    ret = sceUserServiceInitialize(NULL);
    gp_log("[Ghostpad] sceUserServiceInitialize: 0x%08x\n", ret);
    /* Negative here is OK — service may already be initialized by the system */

    /* Log both user service calls so we can compare them */
    int32_t fgUser = -1, initUser = -1;
    int fgRet   = sceUserServiceGetForegroundUser(&fgUser);
    int initRet = sceUserServiceGetInitialUser(&initUser);
    gp_log("[Ghostpad] GetForegroundUser: ret=0x%08x id=0x%08x\n",
           (uint32_t)fgRet, (uint32_t)fgUser);
    gp_log("[Ghostpad] GetInitialUser:    ret=0x%08x id=0x%08x\n",
           (uint32_t)initRet, (uint32_t)initUser);

    /* Prefer initial user — it's the primary logged-in user (typically 0x10000000).
     * GetForegroundUser in a system-service context may return a non-pad-accessible id. */
    if (initRet == 0 && initUser >= 0)
        userId = initUser;
    else if (fgRet == 0 && fgUser >= 0)
        userId = fgUser;
    else
        userId = 0x10000000;

    gp_log("[Ghostpad] using userId = 0x%08x\n", userId);
    ghostpad_notify("Ghostpad: userId 0x%08x", userId);

    /* ---- Phase 2: Validate userId ----
     *
     * GetForegroundUser from a system-service context returns session-context
     * IDs like 0x152fc488 that direct scePad IPC calls (scePadOpen, VirtualDevice)
     * do not accept.  Clamp to the canonical first-user value for those calls.
     *
     * IMPORTANT: keep the original (foreground) userId for the injected
     * SceShellCore/SceShellUI Mbus flow. Direct scePad calls still use the
     * clamped canonical value below. */
    injectUserId = userId;
    if ((uint32_t)userId < 0x10000000u || (uint32_t)userId > 0x1000000Fu) {
        gp_log("[Ghostpad] userId 0x%08x outside [0x10000000,0x1000000F], clamping for direct calls\n",
               (uint32_t)userId);
        userId = 0x10000000;
    } else {
        injectUserId = userId;  /* already canonical, both vars the same */
    }
    gp_log("[Ghostpad] directUserId=0x%08x  injectUserId=0x%08x\n",
           (uint32_t)userId, (uint32_t)injectUserId);

    /* ---- Elevate credentials ---- */
    elevate_credentials();

    /* Stay on GAME authid (0x3800000000010003) set by elevate_credentials().
     * The pad service IPC allow-list requires game authid for scePadGetHandle,
     * scePadVirtualDeviceInsertData, etc. Kernel-debugger authid breaks these.
     * mdbg_copyin is elevated locally in shellui_pad_relaunch_stub_with_handle. */
    gp_log("[Ghostpad] authid: game (pad service calls OK)\n");

    /* ---- Set Pad Process Privilege ---- */
    ret = scePadSetProcessPrivilege(1);
    gp_log("[Ghostpad] scePadSetProcessPrivilege(1): 0x%08x\n", ret);

    /* ---- USB Controller Manager (hotplug + VDA per USB device) ---- */
#if GHOSTPAD_ENABLE_USB_CONTROLLERS
    {
        int cm_ret = ctrl_manager_init(userId, injectUserId);
        gp_log("[Ghostpad] ctrl_manager_init: %d\n", cm_ret);
        if (cm_ret == 0)
            ctrl_manager_start();
    }
#endif

    /* ---- Always-on /dev/klog capture and TCP bridge ----
     * The klog module handles /dev/klog reading, MBus event parsing,
     * TCP bridge for external consumers, and dispatches DEVICE_ADDED
     * device IDs to registered callbacks (Ghostpad VDA tracker + USB
     * controller manager). */
    klog_init();
    klog_on_device(ghostpad_vda_callback, NULL);
#if GHOSTPAD_ENABLE_USB_CONTROLLERS
    klog_on_device(ctrl_manager_on_device_id, NULL);
#endif


    /* Patch this payload process' libScePad before the direct VDA attempts.
     * The SceShellCore manifest patch is still applied later for the injected
     * path, but PT_ATTACH can fail on PS4. Patching self lets the immediate
     * scePadVirtualDeviceAddDevice() attempt use the same verified VDA fix. */
#if GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH
    gp_log("[Ghostpad] Early self VDA patch: dry run...\n");
    int self_pdry = shellui_pad_patch_vda_self(1);
    gp_log("[Ghostpad] self patch_vda dry: matches=%d\n", self_pdry);
    if (self_pdry > 0) {
        int self_p = shellui_pad_patch_vda_self(0);
        gp_log("[Ghostpad] self patch_vda applied: patches=%d\n", self_p);
    }
#endif

    /*
     * =====================================================================================
     *            OPTIONAL SceShellCore VDA PATCHING / SETPRIV HOOK
     * =====================================================================================
     */
#if GHOSTPAD_ENABLE_STARTUP_VDA_PATCH
    gp_log("[Ghostpad] Startup VDA Patching: attempting to patch SceShellCore...\n");
    int pdry = shellui_pad_patch_vda(1);
    gp_log("[Ghostpad] Startup VDA Patching: dry run returned %d matches\n", pdry);
    if (pdry > 0) {
        gp_log("[Ghostpad] Startup VDA Patching: applying VDA patch...\n");
        int p = shellui_pad_patch_vda(0);
        gp_log("[Ghostpad] Startup VDA Patching: patch applied, count = %d\n", p);
        ghostpad_notify("Ghostpad: VDA patch applied %d", p);
    }
#else
    gp_log("[Ghostpad] Startup VDA patch disabled (safe baseline)\n");
#endif

#if GHOSTPAD_ENABLE_SETPRIV_HOOK
    gp_log("[Ghostpad] Startup Privilege Hooking: attempting to hook SceShellCore's scePadSetProcessPrivilege...\n");
    int hret = shellui_pad_hook_setpriv();
    gp_log("[Ghostpad] Startup Privilege Hooking: hook returned %d\n", hret);
#else
    gp_log("[Ghostpad] Startup setpriv hook disabled (safe baseline)\n");
#endif

    /* ---- Acquire pad handle ---- */

    /* scePadGetHandle — get existing handle without opening a new IPC connection */
    padHandle = scePadGetHandle(userId, 0, 0);
    gp_log("[Ghostpad] scePadGetHandle(type=0): 0x%08x\n", (uint32_t)padHandle);

    if (padHandle < 0) {
        /* Also try index 1 in case the user's pad is on slot 1 */
        padHandle = scePadGetHandle(userId, 0, 1);
        gp_log("[Ghostpad] scePadGetHandle(type=0,idx=1): 0x%08x\n", (uint32_t)padHandle);
    }

    /* scePadGetHandle type=3 — find an existing virtual DualSense from a prior run */
    if (padHandle < 0) {
        for (int _idx = 0; _idx < 4 && padHandle < 0; _idx++) {
            padHandle = scePadGetHandle(userId, 3, _idx);
            gp_log("[Ghostpad] scePadGetHandle(type=3,idx=%d): 0x%08x\n",
                   _idx, (uint32_t)padHandle);
        }
        if (padHandle >= 0) {
            gp_log("[Ghostpad] found existing virtual DualSense handle=%d\n", padHandle);
        }
    }

    if (padHandle >= 0) {
        gp_log("[Ghostpad] scePadGetHandle succeeded: handle=%d\n", padHandle);
    }

    /* scePadOpen — standard physical pad open */
    if (padHandle < 0) {
    padHandle = scePadOpen(userId, 0, 0, NULL);
    gp_log("[Ghostpad] scePadOpen(type=0): 0x%08x\n", (uint32_t)padHandle);

    if (padHandle >= 0) {
    }

    if (padHandle < 0) {
        /* scePadVirtualDeviceAddDevice — handle may be in struct fields, not return value */
        struct {
            int32_t size;    /* offset  0 — struct size */
            int32_t userId;  /* offset  4 — user ID     */
            int32_t pad[6];  /* offset  8..31            */
        } vd_param;

        /* Pre-clean orphaned virtual devices from a previous run */
        gp_log("[Ghostpad] VDA pre-clean: deleteDevice(0-511)\n");
        for (int dh = 0; dh < 512; dh++) {
            int32_t dr = scePadVirtualDeviceDeleteDevice(dh);
            if (dr == 0)
                gp_log("[Ghostpad] deleteDevice(%d): OK\n", dh);
            else if (dr != (int32_t)0x803b0003 && dr != (int32_t)0x803b0001)
                gp_log("[Ghostpad] deleteDevice(%d): 0x%08x\n", dh, (uint32_t)dr);
        }

        /* Sentinel pattern to detect which struct fields VDA writes to */
        int vda_created_without_handle = 0;
        const int32_t VDA_SENTINEL = (int32_t)0xDEADBEEF;
        memset(&vd_param, 0, sizeof(vd_param));
        vd_param.size   = (int32_t)sizeof(vd_param);
        vd_param.userId = ghostpad_vda_add_user_id(userId);
        for (int k = 0; k < 6; k++) vd_param.pad[k] = VDA_SENTINEL;

        gp_log("[Ghostpad] VDA call: size=%d userId=0x%08x bindUser=0x%08x type=%s%s\n",
               vd_param.size, (uint32_t)vd_param.userId, (uint32_t)injectUserId,
               (virtual_device_type == VIRTUAL_DEVICE_TYPE_DUALSENSE) ? "DualSense/RemotePlay" : "DS4-compatible");

#if defined(__ORBIS__) && GHOSTPAD_ENABLE_ORBIS_SETPRIV_VDA_SENTINEL
        ret = scePadSetProcessPrivilege(0xDEADBEEF);
#else
        ret = scePadVirtualDeviceAddDevice(&vd_param, virtual_device_type);
#endif

        /* VDA writes handle to struct field, not return value — log all fields */
        gp_log("[Ghostpad] VDA ret=0x%08x  size=%d uid=0x%08x\n",
               (uint32_t)ret, vd_param.size, (uint32_t)vd_param.userId);
        gp_log("[Ghostpad] VDA pad[0-3]=0x%08x 0x%08x 0x%08x 0x%08x\n",
               (uint32_t)vd_param.pad[0], (uint32_t)vd_param.pad[1],
               (uint32_t)vd_param.pad[2], (uint32_t)vd_param.pad[3]);
        gp_log("[Ghostpad] VDA pad[4-5]=0x%08x 0x%08x\n",
               (uint32_t)vd_param.pad[4], (uint32_t)vd_param.pad[5]);

        /* VDA returns a status code.  After the VDA return-value patch this is
         * expected to be 0, but 0 is not a usable pad handle.  Only accept a
         * handle if libScePad actually writes one into the parameter buffer. */
        for (int k = 0; k < 6; k++) {
            if (vd_param.pad[k] != VDA_SENTINEL && vd_param.pad[k] > 0) {
                gp_log("[Ghostpad] VDA handle found in pad[%d] = %d\n",
                       k, vd_param.pad[k]);
                padHandle = vd_param.pad[k];
                break;
            }
        }
        if (ghostpad_vda_ret_can_create_mbus_device(ret) && padHandle < 0) {
            vda_created_without_handle = 1;
            vda_pending_mbus_bind = 1;
            gp_log("[Ghostpad] VDA returned status 0x%08x but may have created an MBus device; waiting for klog DeviceId path\n",
                   (uint32_t)ret);
        }

        /* Retry VDA with SceShellCore authid only if the first call did not
         * successfully create a device.  If the patched first call returns
         * success without a handle, a device may already be present on MBus;
         * a second VDA call just creates another orphan RemotePlay device. */
        if (padHandle < 0 && !vda_created_without_handle) {
            pid_t mypid2 = getpid();
            uint64_t saved_authid2 = kernel_get_ucred_authid(mypid2);
            kernel_set_ucred_authid(mypid2, 0x4800000000000010l);

            memset(&vd_param, 0, sizeof(vd_param));
            vd_param.size   = (int32_t)sizeof(vd_param);
            vd_param.userId = ghostpad_vda_add_user_id(userId);
            for (int k = 0; k < 6; k++) vd_param.pad[k] = VDA_SENTINEL;

#if defined(__ORBIS__) && GHOSTPAD_ENABLE_ORBIS_SETPRIV_VDA_SENTINEL
            int32_t vda2 = scePadSetProcessPrivilege(0xDEADBEEF);
#else
            int32_t vda2 = scePadVirtualDeviceAddDevice(&vd_param, virtual_device_type);
#endif
            gp_log("[Ghostpad] VDA(shellcore-authid) ret=0x%08x pad[0-3]=0x%08x 0x%08x 0x%08x 0x%08x\n",
                   (uint32_t)vda2,
                   (uint32_t)vd_param.pad[0], (uint32_t)vd_param.pad[1],
                   (uint32_t)vd_param.pad[2], (uint32_t)vd_param.pad[3]);

            kernel_set_ucred_authid(mypid2, saved_authid2);

            for (int k = 0; k < 6; k++) {
                if (vd_param.pad[k] != VDA_SENTINEL && vd_param.pad[k] > 0) {
                    gp_log("[Ghostpad] VDA(shellcore) handle in pad[%d]=%d\n",
                           k, vd_param.pad[k]);
                    padHandle = vd_param.pad[k];
                    break;
                }
            }
            if (ghostpad_vda_ret_can_create_mbus_device(vda2) && padHandle < 0) {
                vda_created_without_handle = 1;
                vda_pending_mbus_bind = 1;
                gp_log("[Ghostpad] VDA(shellcore) returned status 0x%08x but may have created an MBus device; waiting for klog DeviceId path\n",
                       (uint32_t)vda2);
            }
        } else if (padHandle < 0 && vda_created_without_handle) {
            gp_log("[Ghostpad] VDA created/accepted without local handle; skipping second VDA call to avoid duplicate orphan device\n");
        }

        if (padHandle >= 0) {
            gp_log("[Ghostpad] VDA handle: %d\n", padHandle);

            /* Auto-press Cross to dismiss the "who is using this controller?" dialog */
            {
                ScePadData ap;
                gp_pad_neutral(&ap);
                ap.buttons = SCE_PAD_BUTTON_CROSS;

                sleep(1);  /* wait for PS5 UI dialog to render */
                ret = scePadVirtualDeviceInsertData(padHandle, &ap);
                gp_log("[Ghostpad] auto-press Cross: VDI ret=0x%08x\n", (uint32_t)ret);
                usleep(200000); /* 200ms hold */

                /* release */
                ap.buttons = 0;
                scePadVirtualDeviceInsertData(padHandle, &ap);
                usleep(100000);
            }
        } else {
            /* All direct pad methods failed; process injection attempted after listen() */
            gp_log("[Ghostpad] All direct pad methods failed; will try injection after listen()\n");
        }
    } /* end approach 3 if (padHandle < 0) */
    } /* end approach 1 outer if (padHandle < 0) */

    /* ---- Create TCP Server Socket ---- */
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        gp_log("[Ghostpad] socket() failed: errno=%d\n", errno);
        goto cleanup;
    }

    /* Allow port reuse so we can restart the payload without waiting.
     * SO_REUSEPORT is disabled to prevent multiple active socket conflicts. */
    int optval = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,  &optval, sizeof(optval));
    /* setsockopt(serverFd, SOL_SOCKET, SO_REUSEPORT,  &optval, sizeof(optval)); */
    setsockopt(serverFd, SOL_SOCKET, SO_NOSIGPIPE,  &optval, sizeof(optval));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(GP_PORT);

    {
        int bind_ok = 0;
        for (int bi = 0; bi < 10; bi++) {
            if (bind(serverFd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                bind_ok = 1;
                break;
            }
            gp_log("[Ghostpad] bind() errno=%d, retry %d/10...\n", errno, bi + 1);
            sleep(2);
        }
        if (!bind_ok) {
            gp_log("[Ghostpad] bind() failed after retries; continuing diagnostic-only without GPAD server\n");
            close(serverFd);
            serverFd = -1;
        }
    }

    if (serverFd >= 0 && listen(serverFd, 1) < 0) {
        gp_log("[Ghostpad] listen() failed: errno=%d\n", errno);
        close(serverFd);
        serverFd = -1;
    }

    if (serverFd >= 0) {
        gp_log("[Ghostpad] Listening on 0.0.0.0:%d\n", GP_PORT);
        ghostpad_notify("Ghostpad: Listening on port %d", GP_PORT);
    }

    /* ---- Control server on port 6970 (non-blocking) for TYPE/GBND/HVDI/DISC ---- */
    {
        ctrlFd = socket(AF_INET, SOCK_STREAM, 0);
        if (ctrlFd >= 0) {
            int ov = 1;
            setsockopt(ctrlFd, SOL_SOCKET, SO_REUSEADDR, &ov, sizeof(ov));
            /* setsockopt(ctrlFd, SOL_SOCKET, SO_REUSEPORT, &ov, sizeof(ov)); */
            setsockopt(ctrlFd, SOL_SOCKET, SO_NOSIGPIPE, &ov, sizeof(ov));
            struct sockaddr_in ca;
            memset(&ca, 0, sizeof(ca));
            ca.sin_family = AF_INET;
            ca.sin_addr.s_addr = INADDR_ANY;
            ca.sin_port = htons(GP_CTRL_PORT);
            if (bind(ctrlFd, (struct sockaddr *)&ca, sizeof(ca)) == 0 &&
                listen(ctrlFd, 8) == 0) {
                /* Set non-blocking so we can poll during the injection wait loop */
                int flags = fcntl(ctrlFd, F_GETFL, 0);
                if (flags >= 0) fcntl(ctrlFd, F_SETFL, flags | O_NONBLOCK);
                gp_log("[Ghostpad] Control port %d ready (GBND)\n", GP_CTRL_PORT);
            } else {
                close(ctrlFd);
                ctrlFd = -1;
                gp_log("[Ghostpad] Control port bind failed errno=%d\n", errno);
            }
        }
    }

    /* ---- If VDA already created a device but did not expose a local handle,
     * wait for ESP32/klog automation to send GBND on the control port.  In this
     * mode PT_ATTACH injection is usually unnecessary and often unavailable. */
    if (padHandle < 0 && shellui_args == 0 && vda_pending_mbus_bind) {
        gp_log("[Ghostpad] VDA device pending MBus bind; waiting for GBND on port %d before injection fallback\n", GP_CTRL_PORT);
        for (int bw = 0; bw < 300 && padHandle < 0; bw++) { /* 60s */
            if (ghostpad_try_klog_candidate_bind(injectUserId) > 0) {
                break;
            }

            if (ghostpad_ctrl_try_bind_once(ctrlFd, injectUserId) > 0) {
                break;
            }
            usleep(200000);
        }
        if (padHandle >= 0) {
            gp_log("[Ghostpad] GBND completed before injection; using direct VDI handle=0x%x\n", (uint32_t)padHandle);
        } else {
            gp_log("[Ghostpad] GBND wait ended without handle; skipping PT_ATTACH injection for VDA self-patch path\n");
        }
    }

    /* ---- Process injection — attempted after listen() so server is already reachable ---- */
    if (padHandle < 0 && shellui_args == 0 && !vda_pending_mbus_bind) {
        if (ctrlFd >= 0) {
            gp_log("[Ghostpad] Waiting briefly for TYPE config on port %d\n", GP_CTRL_PORT);
            for (int tw = 0; tw < 25; tw++) {
                int cfd = accept(ctrlFd, NULL, NULL);
                if (cfd >= 0) {
                    char magic[4];
                    set_recv_timeout_ms(cfd, 250);
                    int nr4 = recv_exact(cfd, magic, 4);
                    if (nr4 == 4 && memcmp(magic, "TYPE", 4) == 0) {
                        GhostpadTypePacket tpkt;
                        memcpy(tpkt.magic, magic, 4);
                        if (recv_exact(cfd, (char *)&tpkt + 4, GP_TYPE_PACKET_SIZE - 4) ==
                            (int)(GP_TYPE_PACKET_SIZE - 4)) {
                            virtual_device_type = (int32_t)tpkt.deviceType;
                            gp_log("[Ghostpad] TYPE: virtual_device_type=%d\n",
                                   virtual_device_type);
                        }
                    }
                    close(cfd);
                }
                usleep(200000);
            }
        }

        /* Optional libScePad VDA patch in SceShellCore. This is no longer a
         * pattern-guess patch: shellui_pad_patch_vda() validates the exact PS4
         * vda_probe fingerprint before writing. */
#if GHOSTPAD_ENABLE_RUNTIME_VDA_PATCH
        gp_log("[Ghostpad] PATH 2: patching libScePad VDA in SceShellCore (dry run first)...\n");
        int pdry = shellui_pad_patch_vda(1);
        gp_log("[Ghostpad] patch_vda dry: matches=%d\n", pdry);
        if (pdry > 0) {
            gp_log("[Ghostpad] applying VDA patch...\n");
            int p = shellui_pad_patch_vda(0);
            gp_log("[Ghostpad] patch_vda applied: patches=%d\n", p);
            ghostpad_notify("Ghostpad: VDA patch applied %d", p);
        }
#else
        gp_log("[Ghostpad] PATH 2: runtime VDA patch disabled (safe baseline)\n");
#endif

        if (shellui_pad_inject(injectUserId, 1, virtual_device_type,
                               &shellui_pid, &shellui_args) == 0) {
            gp_log("[Ghostpad] Injection OK  pid=%d args=0x%lx\n",
                   shellui_pid, shellui_args);
            /* Wait 3s for SceShellCore stub to create the device */
            usleep(3000000);
            /* Poll for stub ready, and also poll ctrlFd for incoming GBND bind commands.
             * Python automation detects DEVICE_ADDED from klog and sends GBND here. */
            int32_t stub_ready = 0;
            int32_t stub_handle = -1;
            int32_t post_ready_polls = -1;
            gp_log("[Ghostpad] entering ctrlFd poll loop, ctrlFd=%d\n", ctrlFd);
            for (int w = 0; w < 325; w++) {  /* 325 × 200ms = 65s */
                /* Non-blocking check for GBND bind command */
                if (ctrlFd >= 0) {
                    int cfd = accept(ctrlFd, NULL, NULL);
                    if (cfd >= 0) {
                        gp_log("[Ghostpad] ctrlFd: accepted fd=%d at iter=%d\n", cfd, w);
                        set_recv_timeout_ms(cfd, 750);
                        /* Read 4-byte magic to decide packet type */
                        char magic[4];
                        int nr4 = recv_exact(cfd, magic, 4);
                        gp_log("[Ghostpad] ctrlFd: magic recv nr4=%d bytes='%c%c%c%c'\n",
                               nr4,
                               nr4>=1?magic[0]:'?', nr4>=2?magic[1]:'?',
                               nr4>=3?magic[2]:'?', nr4>=4?magic[3]:'?');
                        if (nr4 == 4 && memcmp(magic, "GBND", 4) == 0) {
                            GhostpadBindPacket bpkt;
                            memcpy(bpkt.magic, magic, 4);
                            int nr = recv_exact(cfd, (char *)&bpkt + 4, GP_BIND_PACKET_SIZE - 4);
                            close(cfd);
                            if (nr == (int)(GP_BIND_PACKET_SIZE - 4)) {
                                uint64_t vDevId   = bpkt.virtualDevId;
                                uint64_t pDevId   = bpkt.physicalDevId;
                                int32_t  bindUid  = (bpkt.userId != 0) ? (int32_t)bpkt.userId
                                                                        : injectUserId;
                                gp_log("[Ghostpad] GBND: virt=0x%llx phys=0x%llx user=0x%x\n",
                                       (unsigned long long)vDevId,
                                       (unsigned long long)pDevId, (uint32_t)bindUid);
                                bound_virtual_device_id = vDevId;

                                /* Step 1: evict physical device before binding virtual */
                                if (pDevId != 0) {
                                    int dr = shellui_pad_disconnect_device(pDevId);
                                    gp_log("[Ghostpad] disconnect_device ret=%d\n", dr);
                                    usleep(200000); /* 200ms for disconnect to propagate */
                                }

                                /* Step 2: bind virtual device to user */
                                int br = shellui_pad_force_bind(vDevId, bindUid);
                                gp_log("[Ghostpad] force_bind ret=%d\n", br);

                                if (br == 0) {
                                    int32_t dev_handle = (int32_t)(vDevId & 0xffffffffu);
                                    gp_log("[Ghostpad] force_bind succeeded; testing VDI (neutral) handle=0x%x\n",
                                           (uint32_t)dev_handle);
                                    /* Test VDI with neutral state — confirms VDI works without UI input */
                                    int dr = shellcore_pad_test_vdi_neutral(dev_handle);
                                    gp_log("[Ghostpad] GBND: deviceId vdi_neutral ret=%d\n", dr);
                                    if (dr == 0) {
                                        vdi_handle = dev_handle;
                                        device_vdi_ready = 1;
                                        if (shellui_pad_direct_adopt_vdi_handle(shellui_pid,
                                                shellui_args, dev_handle) == 0) {
                                            gp_log("[Ghostpad] GBND: adopted deviceId for direct SceShellCore VDI input\n");
                                        } else {
                                            gp_log("[Ghostpad] GBND: direct adopt of deviceId failed\n");
                                        }
                                        /* Re-launch stub with known handle for no-lag mdbg VDI path */
                                        if (padHandle < 0 && shellui_args != 0) {
                                            gp_log("[Ghostpad] GBND: relaunching stub for no-lag mdbg path...\n");
                                            int rl = shellui_pad_relaunch_stub_with_handle(dev_handle);
                                            gp_log("[Ghostpad] GBND: relaunch_stub ret=%d\n", rl);
                                            if (rl == 0) {
                                                /* Poll for stub ready=1 (up to 3s — shellui_stub+fp_vda=NULL: ~500ms) */
                                                for (int rw = 0; rw < 30; rw++) {
                                                    usleep(100000);
                                                    int32_t rdy = (int32_t)mdbg_getint(shellui_pid,
                                                        shellui_args + (intptr_t)offsetof(ShellUiPadArgs, ready));
                                                    if (rdy == 1) {
                                                        gp_log("[Ghostpad] GBND: stub ready! NO-LAG mdbg path active.\n");
                                                        ghostpad_notify("Ghostpad: mdbg VDI active");
                                                        device_vdi_ready = 2; /* sentinel: use mdbg path */
                                                        break;
                                                    }
                                                }
                                                if (device_vdi_ready != 2) {
                                                    gp_log("[Ghostpad] GBND: stub not ready after 3s; falling back to pt_call\n");
                                                }
                                            }
                                        }
                                    } else {
                                        gp_log("[Ghostpad] force_bind succeeded; waiting for HVDI fallback\n");
                                    }

                                    /* Direct VDI from our process using the raw deviceId */
                                    if (padHandle < 0) {
                                        ScePadData vdi_probe;
                                        gp_pad_neutral(&vdi_probe);
                                        int32_t dev_h = (int32_t)(vDevId & 0xffffffffu);
                                        int vr = scePadVirtualDeviceInsertData(dev_h, &vdi_probe);
                                        gp_log("[Ghostpad] direct-VDI from our process: devId=0x%x ret=0x%x\n",
                                               (uint32_t)dev_h, (uint32_t)vr);
                                        if (vr == 0) {
                                            padHandle = dev_h;
                                            gp_log("[Ghostpad] *** ZERO-LAG DIRECT VDI! padHandle=%d ***\n", padHandle);
                                            ghostpad_notify("Ghostpad: process injected VDI=%d", padHandle);
                                        }
                                    }

                                }
                            }
                        } else if (nr4 == 4 && memcmp(magic, "HVDI", 4) == 0) {
                            if (device_vdi_ready) {
                                gp_log("[Ghostpad] HVDI skipped; deviceId VDI already active\n");
                                close(cfd);
                                continue;
                            }
                            /* HVDI: CIM padHandle received — use for VDI input delivery */
                            GhostpadHvdiPacket hpkt;
                            memcpy(hpkt.magic, magic, 4);
                            int nr = recv_exact(cfd, (char *)&hpkt + 4, GP_HVDI_PACKET_SIZE - 4);
                            close(cfd);
                            if (nr == (int)(GP_HVDI_PACKET_SIZE - 4)) {
                                vdi_handle = (int32_t)hpkt.padHandle;
                                gp_log("[Ghostpad] HVDI: vdi_handle=0x%x\n",
                                       (uint32_t)vdi_handle);
                                /* Immediately test: send Cross via pt_call VDI with known handle */
                                if (vdi_handle > 0) {
                                    gp_log("[Ghostpad] HVDI: testing SceShellUI VDI Cross handle=0x%x...\n",
                                           (uint32_t)vdi_handle);
                                    int vr = shellui_pad_test_vdi_cross(vdi_handle);
                                    gp_log("[Ghostpad] HVDI: shellui vdi_cross ret=%d\n", vr);
                                    gp_log("[Ghostpad] HVDI: testing SceShellCore VDI Cross handle=0x%x...\n",
                                           (uint32_t)vdi_handle);
                                    int cr = shellcore_pad_test_vdi_cross(vdi_handle);
                                    gp_log("[Ghostpad] HVDI: shellcore vdi_cross ret=%d\n", cr);
                                    if (vr == 0 || cr == 0) {
                                    } else {
                                        if (bound_virtual_device_id != 0) {
                                            int32_t dev_handle = (int32_t)(bound_virtual_device_id & 0xffffffffu);
                                            gp_log("[Ghostpad] HVDI: testing virtual deviceId as VDI handle=0x%x...\n",
                                                   (uint32_t)dev_handle);
                                            int dr = shellcore_pad_test_vdi_cross(dev_handle);
                                            gp_log("[Ghostpad] HVDI: deviceId vdi_cross ret=%d\n", dr);
                                            if (dr == 0) {
                                                vdi_handle = dev_handle;
                                                device_vdi_ready = 1;
                                                if (shellui_pad_direct_adopt_vdi_handle(shellui_pid,
                                                        shellui_args, dev_handle) == 0) {
                                                    gp_log("[Ghostpad] HVDI: adopted deviceId for direct SceShellCore VDI input\n");
        } else {
            /* VDA created device on MBus without local handle.
             * On PS5 the device is created under userId=1 — try GetHandle there. */
            if (vda_pending_mbus_bind) {
                for (int _uid = 0; _uid < 3 && padHandle < 0; _uid++) {
                    int32_t try_uid = (_uid == 0) ? 1 : (_uid == 1 ? (int32_t)0xffffffff : userId);
                    for (int _idx = 0; _idx < 8 && padHandle < 0; _idx++) {
                        padHandle = scePadGetHandle(try_uid, 3, _idx);
                        if (padHandle >= 0)
                            gp_log("[Ghostpad] post-VDA GetHandle(uid=0x%x,type=3,idx=%d)=0x%x\n",
                                   (uint32_t)try_uid, _idx, padHandle);
                    }
                }
                if (padHandle >= 0) {
                    gp_log("[Ghostpad] recovered VDA handle via GetHandle: %d\n", padHandle);
                    /* Auto-press Cross to dismiss assignment screen */
                    ScePadData ap;
                    gp_pad_neutral(&ap);
                    ap.buttons = SCE_PAD_BUTTON_CROSS;
                    sleep(1);
                    ret = scePadVirtualDeviceInsertData(padHandle, &ap);
                    gp_log("[Ghostpad] auto-press Cross (post-VDA): VDI ret=0x%08x\n", (uint32_t)ret);
                    usleep(200000);
                    ap.buttons = 0;
                    scePadVirtualDeviceInsertData(padHandle, &ap);
                    usleep(100000);
                }
            }
                                                    gp_log("[Ghostpad] HVDI: direct adopt of deviceId failed\n");
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (nr4 == 4 && memcmp(magic, "UNPT", 4) == 0) {
                            ghostpad_notify("Ghostpad: Unpatched and Exited");
                            gp_log("[Ghostpad] UNPT(ctrl): unpatch and exit requested\n");
                            shellui_pad_unpatch();
                            if (padHandle >= 0) {
                                scePadVirtualDeviceDeleteDevice(padHandle);
                            }
                            if (clientFd >= 0) close(clientFd);
                            if (ctrlFd   >= 0) close(ctrlFd);
                            if (serverFd >= 0) close(serverFd);
                            sceUserServiceTerminate();
                            close(cfd);
                            exit(0);
                        } else if (nr4 == 4 && memcmp(magic, "DISC", 4) == 0) {
                            int ddr = -1;
                            if (bound_virtual_device_id != 0) {
                                ddr = shellui_pad_disconnect_device(bound_virtual_device_id);
                            }
                            gp_log("[Ghostpad] DISC(ctrl): dev=0x%llx ret=%d\n",
                                   (unsigned long long)bound_virtual_device_id, ddr);
                            bound_virtual_device_id = 0;
                            vdi_handle = -1;
                            device_vdi_ready = 0;
                            close(cfd);
                        } else {
                            close(cfd);
                        }

                    }
                }
                stub_ready  = (int32_t)mdbg_getint(shellui_pid,
                    shellui_args + (intptr_t)offsetof(ShellUiPadArgs, ready));
                stub_handle = (int32_t)mdbg_getint(shellui_pid,
                    shellui_args + (intptr_t)offsetof(ShellUiPadArgs, pad_handle));
                if (stub_ready != 0) {
                    if (post_ready_polls < 0) {
                        post_ready_polls = 20; /* 4s grace to accept delayed HVDI */
                        gp_log("[Ghostpad] stub ready observed=%d handle=%d; keeping ctrlFd alive briefly\n",
                               stub_ready, stub_handle);
                    } else if (post_ready_polls-- <= 0) {
                        break;
                    }
                }
                usleep(200000);
            }
            gp_log("[Ghostpad] stub ready=%d pad_handle=%d injectUserId=0x%08x\n",
                   stub_ready, stub_handle, (uint32_t)injectUserId);
            if (stub_ready == 1) {
                ghostpad_notify("Ghostpad: inject OK handle=%d", stub_handle);
            } else if (shellui_pad_direct_usable(shellui_pid, shellui_args)) {
            } else {
                /* Log each probe's error code for diagnosis */
                for (int ri = 0; ri < 16; ri++) {
                    int32_t rce = (int32_t)mdbg_getint(shellui_pid,
                        shellui_args + (intptr_t)offsetof(ShellUiPadArgs, rc_log)
                        + (intptr_t)(ri * 4));
                    /* Print all slots unconditionally — 0 values are meaningful.
                     * rc_log[8..15] = vdp.f[0..7] from the first VDA call (uid=1). */
                    gp_log("[Ghostpad] stub rc_log[%d] = 0x%08x\n",
                           ri, (uint32_t)rce);
                }
                ghostpad_notify("Ghostpad: stub failed ready=%d", stub_ready);
            }
        } else {
            gp_log("[Ghostpad] Injection failed — running without pad input\n");
            ghostpad_notify("Ghostpad: no pad handle - check klog");
        }
    }

    if (serverFd < 0) {
        gp_log("[Ghostpad] Diagnostic-only run complete; GPAD server unavailable\n");
        goto cleanup;
    }

    /* ---- Main Accept Loop ---- */
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        gp_log("[Ghostpad] Waiting for connection...\n");
        clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientFd < 0) {
            gp_log("[Ghostpad] accept() failed: errno=%d\n", errno);
            break;
        }

        {
            uint32_t ip = ntohl(clientAddr.sin_addr.s_addr);
        gp_log("[Ghostpad] Connected: %d.%d.%d.%d\n",
               (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
               (ip >> 8)  & 0xFF, ip & 0xFF);
        ghostpad_notify("Ghostpad: PC connected (%d.%d.%d.%d)",
               (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
               (ip >> 8)  & 0xFF, ip & 0xFF);

        /* Retry GetHandle on client connect — VDA device may be findable now */
        if (padHandle < 0 && vda_pending_mbus_bind) {
            for (int _uid = 0; _uid < 3 && padHandle < 0; _uid++) {
                int32_t try_uid = (_uid == 0) ? 1 : (_uid == 1 ? (int32_t)0xffffffff : userId);
                for (int _idx = 0; _idx < 8 && padHandle < 0; _idx++) {
                    padHandle = scePadGetHandle(try_uid, 3, _idx);
                    if (padHandle >= 0)
                        gp_log("[Ghostpad] connect-retry GetHandle(uid=0x%x,type=3,idx=%d)=0x%x\n",
                               (uint32_t)try_uid, _idx, padHandle);
                }
            }
            if (padHandle >= 0) gp_log("[Ghostpad] recovered handle on client connect: %d\n", padHandle);
        }
        }

        /* Disable Nagle for low-latency input */
#ifdef TCP_NODELAY
        setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
#endif

        /* ---- Packet Read Loop ---- */
        GhostpadPacket pkt;
        ScePadData     padData;
        uint64_t       pktCount = 0;
        int            directPadActive = 0;
        stats_reset(); /* reset server-side CPS counter for each new client */

        if (padHandle < 0 && shellui_args != 0 && device_vdi_ready == 2) {
            /* Stub relaunched with known handle — use shellui_pad_update (mdbg+PT_IO fallback). */
            gp_log("[Ghostpad] using NO-LAG mdbg_copyin path (device_vdi_ready=2)\n");
            directPadActive = 0;
        } else if (padHandle < 0 && shellui_args != 0 &&
            shellui_pad_direct_usable(shellui_pid, shellui_args)) {
            gp_log("[Ghostpad] direct VDI send path available (pt_call — may cause lag)\n");
            directPadActive = 1;
        } else if (padHandle < 0 && shellui_args != 0 &&
                   shellui_pad_direct_mode(shellui_pid, shellui_args) == 1) {
            gp_log("[Ghostpad] skipping unsafe direct remote-insert mode; using shared update path\n");
        }

        while (1) {
            int n = recv_exact(clientFd, &pkt, GP_PACKET_SIZE);
            if (n <= 0) {
                gp_log("[Ghostpad] Client disconnected (recv=%d, errno=%d, pkts=%llu)\n",
                            n, errno, (unsigned long long)pktCount);
                break;
            }

            /* Validate magic bytes */
            if (memcmp(pkt.magic, "UNPT", 4) == 0) {
                ghostpad_notify("Ghostpad: Unpatched and Exited");
                gp_log("[Ghostpad] UNPT(gpad): unpatch and exit requested\n");
                shellui_pad_unpatch();
                if (padHandle >= 0) {
                    scePadVirtualDeviceDeleteDevice(padHandle);
                }
                if (clientFd >= 0) close(clientFd);
                if (ctrlFd   >= 0) close(ctrlFd);
                if (serverFd >= 0) close(serverFd);
                sceUserServiceTerminate();
                exit(0);
            }
            if (memcmp(pkt.magic, "DISC", 4) == 0) {
                int ddr = -1;
                if (bound_virtual_device_id != 0) {
                    ddr = shellui_pad_disconnect_device(bound_virtual_device_id);
                }
                gp_log("[Ghostpad] DISC(gpad): dev=0x%llx ret=%d\n",
                       (unsigned long long)bound_virtual_device_id, ddr);
                bound_virtual_device_id = 0;
                /* Keep device_vdi_ready/stub intact — stub still running, reuse on reconnect */
                break;
            }

            if (memcmp(pkt.magic, GP_MAGIC, GP_MAGIC_LEN) != 0) {
                gp_log("[Ghostpad] Bad magic: %02x %02x %02x %02x\n",
                            (uint8_t)pkt.magic[0], (uint8_t)pkt.magic[1],
                            (uint8_t)pkt.magic[2], (uint8_t)pkt.magic[3]);
                continue;
            }

            memset(&padData, 0, sizeof(padData));
            padData.buttons                = ntohl(pkt.buttons);
            padData.leftStick.x            = pkt.lx;
            padData.leftStick.y            = pkt.ly;
            padData.rightStick.x           = pkt.rx;
            padData.rightStick.y           = pkt.ry;
            padData.analogButtons.l2       = pkt.l2;
            padData.analogButtons.r2       = pkt.r2;
            padData.connected              = 1;
            padData.quat.w                 = 1.0f;

            /* Inject into pad — time the VDI call so we can measure its latency */
            {
                /* Auto-press Cross on first packet to select user on PS5 */
                if (pktCount == 0 && padHandle >= 0) {
                    ScePadData cross_pkt;
                    gp_pad_neutral(&cross_pkt);
                    cross_pkt.buttons = SCE_PAD_BUTTON_CROSS;
                    scePadVirtualDeviceInsertData(padHandle, &cross_pkt);
                    gp_log("[Ghostpad] auto-press Cross (first packet)\n");
                    usleep(200000);
                    cross_pkt.buttons = 0;
                    scePadVirtualDeviceInsertData(padHandle, &cross_pkt);
                    usleep(100000);
                }

                int is_press = (padData.buttons != 0);
                struct timeval vdi_t1, vdi_t2;
                uint64_t vdi_us = 0;

                gettimeofday(&vdi_t1, NULL);

                if (padHandle >= 0) {
                    ret = scePadVirtualDeviceInsertData(padHandle, &padData);
                    /* ── Log VDI errors on first packet AND periodically ── */
                    if (ret < 0 && (pktCount == 0 || pktCount % 1000 == 0)) {
                        gp_log("[Ghostpad] VDI InsertData failed: 0x%08x pkt=%llu handle=0x%x\n",
                               ret, (unsigned long long)pktCount, padHandle);
                    }
                } else if (shellui_args != 0) {
                    ret = directPadActive
                        ? shellui_pad_direct_send(shellui_pid, shellui_args,
                                                  &padData, sizeof(padData))
                        : shellui_pad_update(shellui_pid, shellui_args,
                                             &padData, sizeof(padData));
                    if (ret < 0 && (pktCount == 0 || pktCount % 1000 == 0)) {
                        gp_log("[Ghostpad] shellui pad send failed pkt=%llu ret=%d direct=%d\n",
                               (unsigned long long)pktCount, ret, directPadActive);
                    }
                    if (pktCount == 4) {
                        int32_t r5 = (int32_t)mdbg_getint(shellui_pid,
                            shellui_args + (intptr_t)offsetof(ShellUiPadArgs, rc_log) + 20);
                        int32_t r6 = (int32_t)mdbg_getint(shellui_pid,
                            shellui_args + (intptr_t)offsetof(ShellUiPadArgs, rc_log) + 24);
                        gp_log("[Ghostpad] stub rc_log[5]=0x%08x (fp_insert) rc_log[6]=0x%08x (fp_vdi)\n",
                               (uint32_t)r5, (uint32_t)r6);
                    }
                }

                gettimeofday(&vdi_t2, NULL);
                vdi_us = (uint64_t)(vdi_t2.tv_sec  - vdi_t1.tv_sec)  * 1000000ULL +
                         (uint64_t)(vdi_t2.tv_usec - vdi_t1.tv_usec < 0
                             ? 0 : vdi_t2.tv_usec - vdi_t1.tv_usec);

                stats_record(is_press, ret, vdi_us);
            }

            pktCount++;
            /* Log throughput every 1000 packets + last VDI return code */
            if (pktCount % 1000 == 0) {
                gp_log("[Ghostpad] speedup: %llu packets processed (last_vdi_ret=0x%08x)\n",
                       (unsigned long long)pktCount, ret);
            }
        }

        close(clientFd);
        clientFd = -1;

        /* Zero out / center controller state after disconnect */
        gp_pad_neutral(&padData);
        if (padHandle >= 0) {
            scePadVirtualDeviceInsertData(padHandle, &padData);
        } else if (shellui_args != 0) {
            if (directPadActive)
                shellui_pad_direct_send(shellui_pid, shellui_args,
                                        &padData, sizeof(padData));
            else
                shellui_pad_update(shellui_pid, shellui_args,
                                   &padData, sizeof(padData));
        }
        if (directPadActive) {
            shellui_pad_direct_end(shellui_pid, shellui_args);
        }

        ghostpad_notify("Ghostpad: PC disconnected. Listening again...");
    }

cleanup:
    gp_log("[Ghostpad] Cleaning up...\n");
#if GHOSTPAD_ENABLE_USB_CONTROLLERS
    ctrl_manager_cleanup();
#endif
    if (clientFd >= 0) close(clientFd);
    if (ctrlFd   >= 0) close(ctrlFd);
    if (serverFd >= 0) close(serverFd);
    if (padHandle >= 0) {
        scePadVirtualDeviceDeleteDevice(padHandle);
    }
    sceUserServiceTerminate();
    gp_log("[Ghostpad] Done.\n");
    return 0;
}
