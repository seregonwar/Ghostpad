/* SPDX-License-Identifier: GPL-3.0-or-later
 * controller.h — USB controller abstraction module
 *
 * Single public header.  Everything the rest of Ghostpad needs to work
 * with USB controllers is declared here: types, button constants,
 * controller ops, USB helpers, VID/PID defines, and the public API.
 */

#pragma once
#include <stdint.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>

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
