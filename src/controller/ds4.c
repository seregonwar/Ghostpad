/* SPDX-License-Identifier: GPL-3.0-or-later
 * ds4.c — DualShock 4 / DS4-compatible adapter
 */

#include "controller.h"
#include "ctrl_registry.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#define KLOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define KLOG(...) fprintf(stderr, __VA_ARGS__)
#endif

int ds4_is_compatible_vidpid(uint16_t vid, uint16_t pid)
{
    if (vid == VID_SONY || vid == VID_HORI) return 1;
    return ctrl_registry_is_ds4(vid, pid);
}

static const uint8_t HAT_DPAD[9] = {
    SCE_PAD_BUTTON_UP, SCE_PAD_BUTTON_UP|SCE_PAD_BUTTON_RIGHT,
    SCE_PAD_BUTTON_RIGHT, SCE_PAD_BUTTON_DOWN|SCE_PAD_BUTTON_RIGHT,
    SCE_PAD_BUTTON_DOWN, SCE_PAD_BUTTON_DOWN|SCE_PAD_BUTTON_LEFT,
    SCE_PAD_BUTTON_LEFT, SCE_PAD_BUTTON_UP|SCE_PAD_BUTTON_LEFT, 0u,
};

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
    uint32_t btn = 0;
    uint8_t hat = b[5] & 0x0Fu;
    if (hat <= 8) btn |= HAT_DPAD[hat];
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
                    struct usb_fs_endpoint eps[4],
                    int *hs_out, uint8_t *seq_out)
{
    int fd, ii; struct usb_fs_init init; struct usb_fs_open fs_open;
    *hs_out = HS_STREAMING; *seq_out = 0;
    fd = open(dev_path, O_RDWR);
    if (fd < 0) { KLOG("DS4 open fail errno=%d\n", errno); return -1; }
    for (ii=0;ii<4;ii++){int iface=ii; ioctl(fd,USB_IFACE_DRIVER_DETACH,&iface);}
    usleep(100000);
    memset(eps,0,sizeof(eps[0])*4);
    memset(&init,0,sizeof(init)); init.pEndpoints=eps; init.ep_index_max=2;
    if(ioctl(fd,USB_FS_INIT,&init)!=0){KLOG("DS4 FS_INIT fail\n"); close(fd); return -1;}

    memset(&fs_open,0,sizeof(fs_open)); fs_open.ep_index=0;
    fs_open.ep_no=DS4_EP_IN; fs_open.max_bufsize=64; fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){
        memset(&fs_open,0,sizeof(fs_open)); fs_open.ep_index=0;
        fs_open.ep_no=DS4_EP_IN_ALT; fs_open.max_bufsize=64; fs_open.max_frames=1;
        if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){KLOG("DS4 IN fail\n"); goto fail;}
        KLOG("DS4 IN ep=0x81 (HORI/clone)\n");
    }else KLOG("DS4 IN ep=0x84 (Sony)\n");

    memset(&fs_open,0,sizeof(fs_open)); fs_open.ep_index=1;
    fs_open.ep_no=DS4_EP_OUT; fs_open.max_bufsize=64; fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){
        memset(&fs_open,0,sizeof(fs_open)); fs_open.ep_index=1;
        fs_open.ep_no=DS4_EP_OUT_ALT; fs_open.max_bufsize=64; fs_open.max_frames=1;
        ioctl(fd,USB_FS_OPEN,&fs_open);
    }
    *fd_out = fd; return 0;
fail:
    { struct usb_fs_uninit u; memset(&u,0,sizeof(u)); ioctl(fd,USB_FS_UNINIT,&u); }
    close(fd); return -1;
}

static void ds4_deinit(int fd, struct usb_fs_endpoint eps[4])
{
    struct usb_fs_stop sp; struct usb_fs_close fc; struct usb_fs_uninit u;
    memset(&sp,0,sizeof(sp)); sp.ep_index=0; ioctl(fd,USB_FS_STOP,&sp);
    memset(&sp,0,sizeof(sp)); sp.ep_index=1; ioctl(fd,USB_FS_STOP,&sp);
    memset(&fc,0,sizeof(fc)); fc.ep_index=0; ioctl(fd,USB_FS_CLOSE,&fc);
    memset(&fc,0,sizeof(fc)); fc.ep_index=1; ioctl(fd,USB_FS_CLOSE,&fc);
    memset(&u,0,sizeof(u)); ioctl(fd,USB_FS_UNINIT,&u); close(fd);
}

const CtrlOps g_ctrl_ds4_ops = {
    .name="DS4", .init=ds4_init, .parse=ds4_parse, .deinit=ds4_deinit,
};
