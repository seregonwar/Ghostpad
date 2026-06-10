/* SPDX-License-Identifier: GPL-3.0-or-later
 * logitech.c — Logitech generic HID gamepads (RumblePad 2, Dual Action, F-series DInput)
 */

#include "controller.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <dev/usb/usb_endian.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#define KLOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define KLOG(...) fprintf(stderr, __VA_ARGS__)
#endif

/* HID class requests */
#define HID_BMRT_HOST_TO_DEV_IFACE  0x21u
#define HID_SET_IDLE                0x0Au
#define HID_SET_PROTOCOL            0x0Bu
#define HID_PROTOCOL_REPORT         0x0001u
#define HID_IDLE_PERIODIC_8MS       0x0200u

#define LOGI_EP_IN 0x81

void logitech_wake_up(int fd)
{
    struct usb_ctl_request req;
    memset(&req,0,sizeof(req));
    req.ucr_request.bmRequestType=HID_BMRT_HOST_TO_DEV_IFACE;
    req.ucr_request.bRequest=HID_SET_IDLE;
    USETW(req.ucr_request.wValue,HID_IDLE_PERIODIC_8MS);
    USETW(req.ucr_request.wIndex,0); USETW(req.ucr_request.wLength,0);
    int r1=ioctl(fd,USB_DO_REQUEST,&req);
    KLOG("logi SET_IDLE ret=%d\n",r1);

    memset(&req,0,sizeof(req));
    req.ucr_request.bmRequestType=HID_BMRT_HOST_TO_DEV_IFACE;
    req.ucr_request.bRequest=HID_SET_PROTOCOL;
    USETW(req.ucr_request.wValue,HID_PROTOCOL_REPORT);
    USETW(req.ucr_request.wIndex,0); USETW(req.ucr_request.wLength,0);
    int r2=ioctl(fd,USB_DO_REQUEST,&req);
    KLOG("logi SET_PROTOCOL ret=%d\n",r2);
}

static int isqrt32(uint32_t n){if(n<2)return(int)n;uint32_t x=n,y=(x+1)>>1;while(y<x){x=y;y=(x+n/x)>>1;}return(int)x;}

static void logitech_circularize(uint8_t *x, uint8_t *y)
{
    int nx=(int)*x-128, ny=(int)*y-128;
    const int K2=32768, NORM=181;
    int sy=isqrt32((uint32_t)(K2-ny*ny)), sx=isqrt32((uint32_t)(K2-nx*nx));
    int nx_out=nx*sy/NORM, ny_out=ny*sx/NORM;
    int rx=128+nx_out,ry=128+ny_out;
    if(rx<0)rx=0;else if(rx>255)rx=255;
    if(ry<0)ry=0;else if(ry>255)ry=255;
    *x=(uint8_t)rx;*y=(uint8_t)ry;
}

static const uint8_t HAT_DPAD[9]={
    SCE_PAD_BUTTON_UP,SCE_PAD_BUTTON_UP|SCE_PAD_BUTTON_RIGHT,
    SCE_PAD_BUTTON_RIGHT,SCE_PAD_BUTTON_DOWN|SCE_PAD_BUTTON_RIGHT,
    SCE_PAD_BUTTON_DOWN,SCE_PAD_BUTTON_DOWN|SCE_PAD_BUTTON_LEFT,
    SCE_PAD_BUTTON_LEFT,SCE_PAD_BUTTON_UP|SCE_PAD_BUTTON_LEFT,0u,
};

static int logitech_parse(int fd, struct usb_fs_endpoint eps[4],
                          const uint8_t *buf, uint32_t len,
                          ScePadData *out, int *hs, uint8_t *seq)
{
    (void)fd;(void)eps;(void)hs;(void)seq;
    if(len<6)return 0;
    uint8_t lx=buf[0],ly=buf[1],rx=buf[2],ry=buf[3];
    logitech_circularize(&lx,&ly); logitech_circularize(&rx,&ry);
    out->leftStick.x=lx;out->leftStick.y=ly;out->rightStick.x=rx;out->rightStick.y=ry;
    uint32_t btn=0;
    uint8_t hat=buf[4]&0x0Fu; if(hat<=8)btn|=HAT_DPAD[hat];
    if(buf[4]&0x10u)btn|=SCE_PAD_BUTTON_SQUARE; if(buf[4]&0x20u)btn|=SCE_PAD_BUTTON_CROSS;
    if(buf[4]&0x40u)btn|=SCE_PAD_BUTTON_CIRCLE; if(buf[4]&0x80u)btn|=SCE_PAD_BUTTON_TRIANGLE;
    if(buf[5]&0x01u)btn|=SCE_PAD_BUTTON_L1; if(buf[5]&0x02u)btn|=SCE_PAD_BUTTON_R1;
    if(buf[5]&0x04u){btn|=SCE_PAD_BUTTON_L2;out->analogButtons.l2=0xFFu;}
    if(buf[5]&0x08u){btn|=SCE_PAD_BUTTON_R2;out->analogButtons.r2=0xFFu;}
    if(buf[5]&0x10u)btn|=SCE_PAD_BUTTON_SHARE; if(buf[5]&0x20u)btn|=SCE_PAD_BUTTON_OPTIONS;
    if(buf[5]&0x40u)btn|=SCE_PAD_BUTTON_L3; if(buf[5]&0x80u)btn|=SCE_PAD_BUTTON_R3;
    out->buttons=btn;
    return 1;
}

static int logitech_init(const char *dev_path, int *fd_out,
                         struct usb_fs_endpoint eps[4],
                         int *hs_out, uint8_t *seq_out)
{
    int fd; struct usb_fs_init init; struct usb_fs_open fs_open;
    *hs_out=HS_STREAMING; *seq_out=0;
    fd=open(dev_path,O_RDWR); if(fd<0){KLOG("Logitech open fail\n");return -1;}
    for(int iface=0;iface<4;iface++){int i=iface;ioctl(fd,USB_IFACE_DRIVER_DETACH,&i);}
    usleep(100000);
    memset(eps,0,sizeof(eps[0])*4);
    memset(&init,0,sizeof(init));init.pEndpoints=eps;init.ep_index_max=1;
    if(ioctl(fd,USB_FS_INIT,&init)!=0){KLOG("Logitech FS_INIT fail\n");close(fd);return -1;}

    memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=0;fs_open.ep_no=0x84;
    fs_open.max_bufsize=64;fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){
        memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=0;fs_open.ep_no=LOGI_EP_IN;
        fs_open.max_bufsize=64;fs_open.max_frames=1;
        if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){KLOG("Logitech IN fail\n");goto fail;}
    }
    KLOG("Logitech IN ep=0x%02x maxpkt=%u\n",fs_open.ep_no,(unsigned)fs_open.max_packet_length);
    logitech_wake_up(fd);
    *fd_out=fd; return 0;
fail:
    {struct usb_fs_uninit u;memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u);}
    close(fd); return -1;
}

static void logitech_deinit(int fd, struct usb_fs_endpoint eps[4])
{
    (void)eps;
    struct usb_fs_stop sp;struct usb_fs_close fc;struct usb_fs_uninit u;
    memset(&sp,0,sizeof(sp));sp.ep_index=0;ioctl(fd,USB_FS_STOP,&sp);
    memset(&fc,0,sizeof(fc));fc.ep_index=0;ioctl(fd,USB_FS_CLOSE,&fc);
    memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u);close(fd);
}

const CtrlOps g_ctrl_logitech_ops = {
    .name="Logitech HID", .init=logitech_init, .parse=logitech_parse, .deinit=logitech_deinit,
};
