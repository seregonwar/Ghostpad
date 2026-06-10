/* SPDX-License-Identifier: GPL-3.0-or-later
 * logitech.c — Logitech HID gamepads (RumblePad 2, Dual Action, F-series)
 */

#include "controller.h"
#include <string.h>
#include <dev/usb/usb_endian.h>

#define HID_HOST_TO_DEV  0x21u
#define HID_SET_IDLE     0x0Au
#define HID_SET_PROTOCOL 0x0Bu
#define HID_IDLE_8MS     0x0200u
#define LOGI_EP_IN 0x81

void logitech_wake_up(int fd)
{
    struct usb_ctl_request req;
    memset(&req,0,sizeof(req)); req.ucr_request.bmRequestType=HID_HOST_TO_DEV;
    req.ucr_request.bRequest=HID_SET_IDLE; USETW(req.ucr_request.wValue,HID_IDLE_8MS);
    ioctl(fd,USB_DO_REQUEST,&req);
    memset(&req,0,sizeof(req)); req.ucr_request.bmRequestType=HID_HOST_TO_DEV;
    req.ucr_request.bRequest=HID_SET_PROTOCOL; USETW(req.ucr_request.wValue,1);
    ioctl(fd,USB_DO_REQUEST,&req);
}

static int isqrt32(uint32_t n){if(n<2)return(int)n;uint32_t x=n,y=(x+1)>>1;while(y<x){x=y;y=(x+n/x)>>1;}return(int)x;}

static void circularize(uint8_t *x, uint8_t *y){
    int nx=(int)*x-128,ny=(int)*y-128;
    int sy=isqrt32((uint32_t)(32768-ny*ny)),sx=isqrt32((uint32_t)(32768-nx*nx));
    int nx_out=nx*sy/181,ny_out=ny*sx/181;
    int rx=128+nx_out,ry=128+ny_out;
    if(rx<0)rx=0;else if(rx>255)rx=255;if(ry<0)ry=0;else if(ry>255)ry=255;
    *x=(uint8_t)rx;*y=(uint8_t)ry;
}

static int logitech_parse(int fd, struct usb_fs_endpoint eps[4],
                          const uint8_t *buf, uint32_t len,
                          ScePadData *out, int *hs, uint8_t *seq)
{
    (void)fd;(void)eps;(void)hs;(void)seq;
    if(len<6)return 0;
    uint8_t lx=buf[0],ly=buf[1],rx=buf[2],ry=buf[3];
    circularize(&lx,&ly);circularize(&rx,&ry);
    out->leftStick.x=lx;out->leftStick.y=ly;out->rightStick.x=rx;out->rightStick.y=ry;
    uint32_t btn=hat_to_dpad(buf[4]&0x0Fu);
    if(buf[4]&0x10u)btn|=SCE_PAD_BUTTON_SQUARE;if(buf[4]&0x20u)btn|=SCE_PAD_BUTTON_CROSS;
    if(buf[4]&0x40u)btn|=SCE_PAD_BUTTON_CIRCLE;if(buf[4]&0x80u)btn|=SCE_PAD_BUTTON_TRIANGLE;
    if(buf[5]&0x01u)btn|=SCE_PAD_BUTTON_L1;if(buf[5]&0x02u)btn|=SCE_PAD_BUTTON_R1;
    if(buf[5]&0x04u){btn|=SCE_PAD_BUTTON_L2;out->analogButtons.l2=0xFFu;}
    if(buf[5]&0x08u){btn|=SCE_PAD_BUTTON_R2;out->analogButtons.r2=0xFFu;}
    if(buf[5]&0x10u)btn|=SCE_PAD_BUTTON_SHARE;if(buf[5]&0x20u)btn|=SCE_PAD_BUTTON_OPTIONS;
    if(buf[5]&0x40u)btn|=SCE_PAD_BUTTON_L3;if(buf[5]&0x80u)btn|=SCE_PAD_BUTTON_R3;
    out->buttons=btn; return 1;
}

static int logitech_init(const char *dev_path, int *fd_out,
                         struct usb_fs_endpoint eps[4], int *hs_out, uint8_t *seq_out)
{
    *hs_out=HS_STREAMING; *seq_out=0;
    int fd = usb_dev_open(dev_path, eps, 1);
    if (fd < 0) { KLOG("Logitech open fail\n"); return -1; }

    if (usb_ep_open(fd, eps, 0, 0x84, 64) != 0 &&
        usb_ep_open(fd, eps, 0, LOGI_EP_IN, 64) != 0) {
        KLOG("Logitech IN fail\n"); usb_dev_close(fd, eps, 0); return -1;
    }
    logitech_wake_up(fd);
    *fd_out=fd; return 0;
}

static void logitech_deinit(int fd, struct usb_fs_endpoint eps[4]) { usb_dev_close(fd, eps, 1); }

const CtrlOps g_ctrl_logitech_ops = { "Logitech", logitech_init, logitech_parse, logitech_deinit };
