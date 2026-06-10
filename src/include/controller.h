/* SPDX-License-Identifier: GPL-3.0-or-later
 * controller.h — USB controller abstraction module
 *
 * Single public header.  Everything the rest of Ghostpad needs.
 */

#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>

/* ── Logging macro (shared across all controller .c files) ────────────── */
#ifdef __PROSPERO__
#include <ps5/klog.h>
#define KLOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define KLOG(...) fprintf(stderr, __VA_ARGS__)
#endif

/* ── SCE pad button bitmask ───────────────────────────────────────────── */
#define SCE_PAD_BUTTON_SHARE      0x00000001u
#define SCE_PAD_BUTTON_L3         0x00000002u
#define SCE_PAD_BUTTON_R3         0x00000004u
#define SCE_PAD_BUTTON_OPTIONS    0x00000008u
#define SCE_PAD_BUTTON_UP         0x00000010u
#define SCE_PAD_BUTTON_RIGHT      0x00000020u
#define SCE_PAD_BUTTON_DOWN       0x00000040u
#define SCE_PAD_BUTTON_LEFT       0x00000080u
#define SCE_PAD_BUTTON_L2         0x00000100u
#define SCE_PAD_BUTTON_R2         0x00000200u
#define SCE_PAD_BUTTON_L1         0x00000400u
#define SCE_PAD_BUTTON_R1         0x00000800u
#define SCE_PAD_BUTTON_TRIANGLE   0x00001000u
#define SCE_PAD_BUTTON_CIRCLE     0x00002000u
#define SCE_PAD_BUTTON_CROSS      0x00004000u
#define SCE_PAD_BUTTON_SQUARE     0x00008000u
#define SCE_PAD_BUTTON_PS         0x00010000u
#define SCE_PAD_BUTTON_CREATE     0x00010000u
#define SCE_PAD_BUTTON_TOUCH_PAD  0x00100000u

/* ── ScePadData layout (PS5 libScePad) ────────────────────────────────── */
typedef struct {
    uint16_t x; uint16_t y; uint8_t finger; uint8_t pad[3];
} ScePadTouch;
typedef struct {
    uint8_t fingers; uint8_t pad1[3]; uint32_t pad2; ScePadTouch touch[2];
} ScePadTouchData;
typedef struct {
    uint32_t buttons;
    struct { uint8_t x; uint8_t y; } leftStick;
    struct { uint8_t x; uint8_t y; } rightStick;
    struct { uint8_t l2; uint8_t r2; } analogButtons;
    uint16_t    padding;
    struct { float x, y, z, w; } quat;
    struct { float x, y, z; }    vel;
    struct { float x, y, z; }    accel;
    ScePadTouchData touchData;
    uint8_t  connected; uint8_t  _align[3];
    uint64_t timestamp; uint8_t  ext[16];
    uint8_t  count;     uint8_t  unknown[15];
} ScePadData;

/* ── Known USB Vendor/Product IDs ─────────────────────────────────────── */
#define VID_SONY         0x054Cu
#define VID_HORI         0x0F0Du
#define VID_SWITCH       0x057Eu
#define PID_SWITCH       0x2009u
#define VID_XBOX         0x045Eu
#define PID_XBOX         0x02EAu
#define VID_LOGITECH     0x046Du
#define PID_RUMBLEPAD2   0xC218u

/* Third-party PS4-protocol vendor IDs (for ds4_is_compatible_vidpid) */
#define VID_VENOM             0x0079u
#define VID_THRUSTMASTER      0x044Fu
#define VID_MADCATZ           0x0738u
#define VID_BROOK             0x0C12u
#define VID_PDP               0x0E6Fu
#define VID_PS4FUN            0x11C0u
#define VID_NACON             0x146Bu
#define VID_RAZER             0x1532u
#define VID_POWERA            0x20D6u
#define VID_QANBA             0x2C22u
#define VID_NACON_NEW         0x3285u
#define VID_LEVELUP           0x7545u
#define VID_ASTRO             0x9886u

/* Xbox GIP interface signature */
#define XBOX_GIP_SUBCLASS  0x47u
#define XBOX_GIP_PROTOCOL  0xD0u

/* DS4 endpoints */
#define DS4_EP_IN      0x84
#define DS4_EP_IN_ALT  0x81
#define DS4_EP_OUT     0x03
#define DS4_EP_OUT_ALT 0x02

/* ── Maximum report size ──────────────────────────────────────────────── */
#define CTRL_REPORT_MAX 1024u

/* ── Controller operations ────────────────────────────────────────────── */
typedef struct CtrlOps {
    const char *name;
    int  (*init)(const char *dev_path, int *fd_out,
                 struct usb_fs_endpoint eps[4],
                 int *hs_state_out, uint8_t *seq_out);
    int  (*parse)(int fd, struct usb_fs_endpoint eps[4],
                  const uint8_t *buf, uint32_t len, ScePadData *out,
                  int *hs_state, uint8_t *seq);
    void (*deinit)(int fd, struct usb_fs_endpoint eps[4]);
} CtrlOps;

/* ── Controller descriptor flags ─────────────────────────────────────── */
#define CTRL_FLAG_CACHED_STATE   (1u << 0)
#define CTRL_FLAG_PER_READ_STOP  (1u << 1)
#define CTRL_FLAG_POLL_READ      (1u << 2)

typedef struct {
    uint16_t       vid;  uint16_t pid;
    const char    *name;
    const CtrlOps *ops;
    uint32_t       report_size;
    uint32_t       n_frames;
    uint32_t       flags;
} CtrlDesc;

/* ── Handshake state constants ────────────────────────────────────────── */
#define HS_STREAMING   2
#define HS_WAIT_81_01  0
#define HS_WAIT_81_02  1

/* ── Public API ───────────────────────────────────────────────────────── */
const CtrlDesc *ctrl_lookup(uint16_t vid, uint16_t pid);
const CtrlDesc *ctrl_probe(const char *path, uint16_t *out_vid, uint16_t *out_pid);
int ctrl_run(const char *path, const CtrlDesc *desc,
             int32_t vdi_handle, int slot,
             void (*on_frame)(int slot, const ScePadData *pad));

/* ── DS4 third-party compatibility ───────────────────────────────────── */
int ds4_is_compatible_vidpid(uint16_t vid, uint16_t pid);

/* ── USB helpers ──────────────────────────────────────────────────────── */
int usb_send_out(int fd, struct usb_fs_endpoint *ep,
                 const uint8_t *data, uint32_t len, const char *tag);
int usb_send_cmd(int fd, struct usb_fs_endpoint *ep, uint8_t a, uint8_t b);

/* ── PS5 toast notification (implemented in main.c) ───────────────────── */
void ghostpad_notify(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ── Shared inline helpers ────────────────────────────────────────────── */

/* USB device lifecycle: open, optionally detach drivers, FS_INIT.
 * Pass detach=1 for external USB ports, detach=0 for internal bus devices
 * (ugen0/ugen1) to avoid reset loops. */
static inline int usb_dev_open_ex(const char *path, struct usb_fs_endpoint eps[4], int ep_max, int detach) {
    int fd = open(path, O_RDWR);
    if (fd < 0) return -1;
    if (detach) {
        for (int i = 0; i < 4; i++) { int iface = i; ioctl(fd, USB_IFACE_DRIVER_DETACH, &iface); }
        usleep(100000);
    }
    memset(eps, 0, sizeof(eps[0]) * 4);
    struct usb_fs_init init; memset(&init, 0, sizeof(init));
    init.pEndpoints = eps; init.ep_index_max = ep_max;
    if (ioctl(fd, USB_FS_INIT, &init) != 0) { close(fd); return -1; }
    return fd;
}
/* Default: with detach (existing callers unchanged) */
static inline int usb_dev_open(const char *path, struct usb_fs_endpoint eps[4], int ep_max) {
    return usb_dev_open_ex(path, eps, ep_max, 1);
}

/* Open one endpoint.  Returns 0 on success, -1 on error. */
static inline int usb_ep_open(int fd, struct usb_fs_endpoint eps[4], int idx, int ep_no, int maxpkt) {
    (void)eps;
    struct usb_fs_open o; memset(&o, 0, sizeof(o));
    o.ep_index = idx; o.ep_no = ep_no; o.max_bufsize = maxpkt; o.max_frames = 1;
    return ioctl(fd, USB_FS_OPEN, &o);
}

/* Close device: stop+close all eps, FS_UNINIT, close fd. */
static inline void usb_dev_close(int fd, struct usb_fs_endpoint eps[4], int ep_count) {
    (void)eps;
    for (int i = 0; i < ep_count; i++) {
        struct usb_fs_stop  sp; memset(&sp, 0, sizeof(sp)); sp.ep_index = i;
        ioctl(fd, USB_FS_STOP,  &sp);
        struct usb_fs_close fc; memset(&fc, 0, sizeof(fc)); fc.ep_index = i;
        ioctl(fd, USB_FS_CLOSE, &fc);
    }
    struct usb_fs_uninit u; memset(&u, 0, sizeof(u));
    ioctl(fd, USB_FS_UNINIT, &u);
    close(fd);
}

/* Initialise ScePadData to neutral (centered sticks, connected, identity quat) */
static inline void gp_pad_neutral(ScePadData *p) {
    p->buttons = 0;
    p->leftStick.x = 128;  p->leftStick.y  = 128;
    p->rightStick.x = 128; p->rightStick.y = 128;
    p->analogButtons.l2 = 0; p->analogButtons.r2 = 0;
    p->connected = 1;
    p->quat.w = 1.0f;
}

/* Convert 8-direction hat (0=N..7=NW, 8=neutral) to SCE dpad bits */
static inline uint32_t hat_to_dpad(uint8_t hat) {
    static const uint32_t map[9] = {
        SCE_PAD_BUTTON_UP,
        SCE_PAD_BUTTON_UP    | SCE_PAD_BUTTON_RIGHT,
        SCE_PAD_BUTTON_RIGHT,
        SCE_PAD_BUTTON_DOWN  | SCE_PAD_BUTTON_RIGHT,
        SCE_PAD_BUTTON_DOWN,
        SCE_PAD_BUTTON_DOWN  | SCE_PAD_BUTTON_LEFT,
        SCE_PAD_BUTTON_LEFT,
        SCE_PAD_BUTTON_UP    | SCE_PAD_BUTTON_LEFT,
        0u,
    };
    return (hat <= 8) ? map[hat] : 0u;
}

/* Scale 12-bit stick value (0-4095) to 8-bit (0-255) */
static inline uint8_t stick_scale_12to8(uint16_t v) {
    if (v > 4095) v = 4095;
    return (uint8_t)((v * 255u) / 4095u);
}
