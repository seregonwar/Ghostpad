/* SPDX-License-Identifier: GPL-3.0-or-later
 * ds4.c — DualShock 4 / DS4-compatible adapter
 */

#include "controller.h"
#include "ctrl_registry.h"
#include <string.h>

int ds4_is_compatible_vidpid(uint16_t vid, uint16_t pid)
{
    if (vid == VID_SONY || vid == VID_HORI) return 1;
    return ctrl_registry_is_ds4(vid, pid);
}

static int ds4_parse(int fd, struct usb_fs_endpoint eps[4],
                     const uint8_t *buf, uint32_t len,
                     ScePadData *out, int *hs, uint8_t *seq)
{
    (void)fd;(void)eps;(void)hs;(void)seq;
    if (len < 10 || buf[0] != 0x01) return 0;
    const uint8_t *b = buf;
    out->leftStick.x = b[1]; out->leftStick.y = b[2];
    out->rightStick.x = b[3]; out->rightStick.y = b[4];
    out->analogButtons.l2 = b[8]; out->analogButtons.r2 = b[9];
    uint32_t btn = hat_to_dpad(b[5] & 0x0Fu);
    if(b[5]&0x10u)btn|=SCE_PAD_BUTTON_SQUARE; if(b[5]&0x20u)btn|=SCE_PAD_BUTTON_CROSS;
    if(b[5]&0x40u)btn|=SCE_PAD_BUTTON_CIRCLE; if(b[5]&0x80u)btn|=SCE_PAD_BUTTON_TRIANGLE;
    if(b[6]&0x01u)btn|=SCE_PAD_BUTTON_L1; if(b[6]&0x02u)btn|=SCE_PAD_BUTTON_R1;
    if(b[6]&0x04u)btn|=SCE_PAD_BUTTON_L2; if(b[6]&0x08u)btn|=SCE_PAD_BUTTON_R2;
    if(b[6]&0x10u)btn|=SCE_PAD_BUTTON_SHARE; if(b[6]&0x20u)btn|=SCE_PAD_BUTTON_OPTIONS;
    if(b[6]&0x40u)btn|=SCE_PAD_BUTTON_L3; if(b[6]&0x80u)btn|=SCE_PAD_BUTTON_R3;
    if(b[7]&0x01u)btn|=SCE_PAD_BUTTON_PS; if(b[7]&0x02u)btn|=SCE_PAD_BUTTON_TOUCH_PAD;
    out->buttons = btn;
    return 1;
}

static int ds4_init(const char *dev_path, int *fd_out,
                    struct usb_fs_endpoint eps[4], int *hs_out, uint8_t *seq_out)
{
    *hs_out = HS_STREAMING; *seq_out = 0;
    int fd = usb_dev_open(dev_path, eps, 2);
    if (fd < 0) { KLOG("DS4 open fail\n"); return -1; }

    if (usb_ep_open(fd, eps, 0, DS4_EP_IN, 64) != 0 &&
        usb_ep_open(fd, eps, 0, DS4_EP_IN_ALT, 64) != 0) {
        KLOG("DS4 IN fail\n"); usb_dev_close(fd, eps, 0); return -1;
    }
    usb_ep_open(fd, eps, 1, DS4_EP_OUT, 64) ||
    usb_ep_open(fd, eps, 1, DS4_EP_OUT_ALT, 64);  /* optional */

    *fd_out = fd; return 0;
}

static void ds4_deinit(int fd, struct usb_fs_endpoint eps[4]) { usb_dev_close(fd, eps, 2); }

const CtrlOps g_ctrl_ds4_ops = { "DS4", ds4_init, ds4_parse, ds4_deinit };
