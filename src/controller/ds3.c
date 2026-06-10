/* SPDX-License-Identifier: GPL-3.0-or-later
 * ds3.c — DualShock 3 / SIXAXIS native adapter
 * adapted from da-chemizt implementation 
 * Protocol: linux/drivers/hid/hid-sony.c sixaxis_set_operational_usb.
 * The DS3 stays silent until the host sends two HID GET_REPORT control
 * transfers (feature 0xF2 + 0xF5) on EP0. Then streams 49-byte reports
 * (ID 0x01) on EP 0x81.
 */

#include "controller.h"
#include <errno.h>
#include <dev/usb/usb_endian.h>

/* HID class constants */
#define HID_BMRT_DEV_TO_HOST_IFACE  0xA1u
#define HID_GET_REPORT              0x01u
#define HID_FEATURE_TYPE            0x03u  /* high byte of wValue */
#define DS3_EP_IN                   0x81
#define DS3_REPORT_F2_SIZE          17
#define DS3_REPORT_F5_SIZE          8

static int ds3_get_feature(int fd, uint8_t report_id, uint8_t *buf, uint16_t len)
{
    struct usb_ctl_request req;
    memset(&req, 0, sizeof(req));
    req.ucr_request.bmRequestType = HID_BMRT_DEV_TO_HOST_IFACE;
    req.ucr_request.bRequest      = HID_GET_REPORT;
    USETW(req.ucr_request.wValue,  ((uint16_t)HID_FEATURE_TYPE << 8) | report_id);
    USETW(req.ucr_request.wIndex,  0x0000);
    USETW(req.ucr_request.wLength, len);
    req.ucr_data = buf;
    return ioctl(fd, USB_DO_REQUEST, &req);
}

/* Enable streaming: sends two GET_REPORT control transfers. After this the
 * DS3 starts emitting input reports on the interrupt IN endpoint. */
static int ds3_enable_streaming(int fd)
{
    uint8_t buf[DS3_REPORT_F2_SIZE];
    memset(buf, 0, sizeof(buf));
    int r1 = ds3_get_feature(fd, 0xF2, buf, DS3_REPORT_F2_SIZE);
    KLOG("DS3 GET_REPORT(0xF2,17) ret=%d errno=%d\n", r1, r1 ? errno : 0);
    if (r1 != 0) return -1;

    memset(buf, 0, sizeof(buf));
    int r2 = ds3_get_feature(fd, 0xF5, buf, DS3_REPORT_F5_SIZE);
    KLOG("DS3 GET_REPORT(0xF5,8) ret=%d errno=%d\n", r2, r2 ? errno : 0);
    if (r2 != 0) return -1;

    return 0;
}

static int ds3_parse(int fd, struct usb_fs_endpoint eps[4],
                      const uint8_t *buf, uint32_t len,
                      ScePadData *out, int *hs, uint8_t *seq)
{
    (void)fd; (void)eps; (void)hs; (void)seq;

    /* DS3 emits report ID 0x01 at nominally 49 bytes. Need >= 20 (through triggers). */
    if (len < 20 || buf[0] != 0x01)
        return 0;

    out->leftStick.x  = buf[6];
    out->leftStick.y  = buf[7];
    out->rightStick.x = buf[8];
    out->rightStick.y = buf[9];
    out->analogButtons.l2 = buf[18];
    out->analogButtons.r2 = buf[19];

    uint32_t btn = 0;

    /* b[2]: Select/L3/R3/Start + dpad (individual bits, NOT a hat) */
    if (buf[2] & 0x01) btn |= SCE_PAD_BUTTON_SHARE;
    if (buf[2] & 0x02) btn |= SCE_PAD_BUTTON_L3;
    if (buf[2] & 0x04) btn |= SCE_PAD_BUTTON_R3;
    if (buf[2] & 0x08) btn |= SCE_PAD_BUTTON_OPTIONS;
    if (buf[2] & 0x10) btn |= SCE_PAD_BUTTON_UP;
    if (buf[2] & 0x20) btn |= SCE_PAD_BUTTON_RIGHT;
    if (buf[2] & 0x40) btn |= SCE_PAD_BUTTON_DOWN;
    if (buf[2] & 0x80) btn |= SCE_PAD_BUTTON_LEFT;

    /* b[3]: triggers + face buttons */
    if (buf[3] & 0x01) btn |= SCE_PAD_BUTTON_L2;
    if (buf[3] & 0x02) btn |= SCE_PAD_BUTTON_R2;
    if (buf[3] & 0x04) btn |= SCE_PAD_BUTTON_L1;
    if (buf[3] & 0x08) btn |= SCE_PAD_BUTTON_R1;
    if (buf[3] & 0x10) btn |= SCE_PAD_BUTTON_TRIANGLE;
    if (buf[3] & 0x20) btn |= SCE_PAD_BUTTON_CIRCLE;
    if (buf[3] & 0x40) btn |= SCE_PAD_BUTTON_CROSS;
    if (buf[3] & 0x80) btn |= SCE_PAD_BUTTON_SQUARE;

    /* b[4]: PS button (bit 0) */
    if (buf[4] & 0x01) btn |= SCE_PAD_BUTTON_PS;

    /* Digital trigger fallback from analog value */
    if (buf[18] > 16) btn |= SCE_PAD_BUTTON_L2;
    if (buf[19] > 16) btn |= SCE_PAD_BUTTON_R2;

    out->buttons = btn;
    return 1;
}

static int ds3_init(const char *dev_path, int *fd_out,
                     struct usb_fs_endpoint eps[4], int *hs_out, uint8_t *seq_out)
{
    *hs_out = HS_STREAMING; *seq_out = 0;

    int fd = usb_dev_open(dev_path, eps, 1);
    if (fd < 0) { KLOG("DS3 open fail\n"); return -1; }

    /* Operational handshake on EP0 — both GET_REPORTs must succeed
     * before opening the IN endpoint, otherwise the device sits idle. */
    if (ds3_enable_streaming(fd) != 0) {
        KLOG("DS3 enable_streaming failed\n");
        usb_dev_close(fd, eps, 0); return -1;
    }

    /* Open IN endpoint AFTER the handshake */
    if (usb_ep_open(fd, eps, 0, DS3_EP_IN, 64) != 0) {
        KLOG("DS3 IN open fail\n"); usb_dev_close(fd, eps, 0); return -1;
    }
    KLOG("DS3 IN ok ep=0x%02x\n", DS3_EP_IN);

    *fd_out = fd; return 0;
}

static void ds3_deinit(int fd, struct usb_fs_endpoint eps[4]) { usb_dev_close(fd, eps, 1); }

const CtrlOps g_ctrl_ds3_ops = { "DS3", ds3_init, ds3_parse, ds3_deinit };
