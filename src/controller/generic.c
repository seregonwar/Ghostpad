/* SPDX-License-Identifier: GPL-3.0-or-later
 * generic.c — Universal HID/USB gamepad fallback
 */

#include "controller.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#define KLOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define KLOG(...) fprintf(stderr, __VA_ARGS__)
#endif

#define GENERIC_EP_FIRST_IN  0x81u
#define GENERIC_EP_LAST_IN   0x8fu

#define GEN_FMT_UNKNOWN         100
#define GEN_FMT_DS4_LIKE        101
#define GEN_FMT_SWITCH_SIMPLE   102
#define GEN_FMT_SWITCH_FULL     103
#define GEN_FMT_HID_4A_HAT_BTN  104
#define GEN_FMT_HID_6A_HAT_BTN  105
#define GEN_FMT_HID_4A_BTN      106

static const uint32_t HAT_DPAD[9]={
    SCE_PAD_BUTTON_UP,SCE_PAD_BUTTON_UP|SCE_PAD_BUTTON_RIGHT,
    SCE_PAD_BUTTON_RIGHT,SCE_PAD_BUTTON_DOWN|SCE_PAD_BUTTON_RIGHT,
    SCE_PAD_BUTTON_DOWN,SCE_PAD_BUTTON_DOWN|SCE_PAD_BUTTON_LEFT,
    SCE_PAD_BUTTON_LEFT,SCE_PAD_BUTTON_UP|SCE_PAD_BUTTON_LEFT,0u,
};
static const uint32_t BUTTON_BITS[16]={
    SCE_PAD_BUTTON_CROSS,SCE_PAD_BUTTON_CIRCLE,SCE_PAD_BUTTON_SQUARE,SCE_PAD_BUTTON_TRIANGLE,
    SCE_PAD_BUTTON_L1,SCE_PAD_BUTTON_R1,SCE_PAD_BUTTON_L2,SCE_PAD_BUTTON_R2,
    SCE_PAD_BUTTON_SHARE,SCE_PAD_BUTTON_OPTIONS,SCE_PAD_BUTTON_L3,SCE_PAD_BUTTON_R3,
    SCE_PAD_BUTTON_PS,SCE_PAD_BUTTON_TOUCH_PAD,0u,0u,
};

static void pad_defaults(ScePadData *o){o->leftStick.x=128;o->leftStick.y=128;o->rightStick.x=128;o->rightStick.y=128;}
static uint32_t hat_to_dpad(uint8_t h){h&=0xf;return(h<=8)?HAT_DPAD[h]:0u;}
static uint32_t map_button_bits(uint16_t bits){uint32_t b=0;for(unsigned i=0;i<16;i++)if(bits&(1u<<i))b|=BUTTON_BITS[i];return b;}

static uint8_t maybe_off(const uint8_t *b,uint32_t l){
    if(l<7)return 0;
    if((b[0]>=1&&b[0]<=8)||b[0]==0x11||b[0]==0x30||b[0]==0x3f)return 1;
    return 0;
}

static int parse_ds4_like(const uint8_t *b,uint32_t l,ScePadData *o){
    if(l<10||b[0]!=0x01||(b[5]&0xf)>8)return 0;
    o->leftStick.x=b[1];o->leftStick.y=b[2];o->rightStick.x=b[3];o->rightStick.y=b[4];
    o->analogButtons.l2=b[8];o->analogButtons.r2=b[9];
    uint32_t btn=hat_to_dpad(b[5]);
    if(b[5]&0x10u)btn|=SCE_PAD_BUTTON_SQUARE;if(b[5]&0x20u)btn|=SCE_PAD_BUTTON_CROSS;
    if(b[5]&0x40u)btn|=SCE_PAD_BUTTON_CIRCLE;if(b[5]&0x80u)btn|=SCE_PAD_BUTTON_TRIANGLE;
    if(b[6]&0x01u)btn|=SCE_PAD_BUTTON_L1;if(b[6]&0x02u)btn|=SCE_PAD_BUTTON_R1;
    if(b[6]&0x04u)btn|=SCE_PAD_BUTTON_L2;if(b[6]&0x08u)btn|=SCE_PAD_BUTTON_R2;
    if(b[6]&0x10u)btn|=SCE_PAD_BUTTON_SHARE;if(b[6]&0x20u)btn|=SCE_PAD_BUTTON_OPTIONS;
    if(b[6]&0x40u)btn|=SCE_PAD_BUTTON_L3;if(b[6]&0x80u)btn|=SCE_PAD_BUTTON_R3;
    if(b[7]&0x01u)btn|=SCE_PAD_BUTTON_PS;if(b[7]&0x02u)btn|=SCE_PAD_BUTTON_TOUCH_PAD;
    o->buttons=btn;return 1;
}

static uint8_t scale12(uint16_t v){if(v>4095u)v=4095u;return(uint8_t)((v*255u)/4095u);}

static int parse_switch_full(const uint8_t *b,uint32_t l,ScePadData *o){
    if(!((b[0]==0x30||b[0]==0x00)&&l>=12))return 0;
    if(b[0]==0x00&&b[1]==0)return 0;
    uint8_t br=b[3],bs=b[4],bl=b[5];
    uint16_t lx=(uint16_t)(b[6]|((b[7]&0xf)<<8)),ly=(uint16_t)((b[7]>>4)|((uint16_t)b[8]<<4));
    uint16_t rx=(uint16_t)(b[9]|((b[10]&0xf)<<8)),ry=(uint16_t)((b[10]>>4)|((uint16_t)b[11]<<4));
    uint32_t btn=0;
    if(br&0x04u)btn|=SCE_PAD_BUTTON_CROSS;if(br&0x08u)btn|=SCE_PAD_BUTTON_CIRCLE;
    if(br&0x01u)btn|=SCE_PAD_BUTTON_SQUARE;if(br&0x02u)btn|=SCE_PAD_BUTTON_TRIANGLE;
    if(bl&0x40u)btn|=SCE_PAD_BUTTON_L1;if(bl&0x80u)btn|=SCE_PAD_BUTTON_L2;
    if(br&0x40u)btn|=SCE_PAD_BUTTON_R1;if(br&0x80u)btn|=SCE_PAD_BUTTON_R2;
    if(bs&0x08u)btn|=SCE_PAD_BUTTON_L3;if(bs&0x04u)btn|=SCE_PAD_BUTTON_R3;
    if(bs&0x02u)btn|=SCE_PAD_BUTTON_OPTIONS;if(bs&0x01u)btn|=SCE_PAD_BUTTON_CREATE;
    if(bs&0x10u)btn|=SCE_PAD_BUTTON_PS;if(bs&0x20u)btn|=SCE_PAD_BUTTON_TOUCH_PAD;
    if(bl&0x02u)btn|=SCE_PAD_BUTTON_UP;if(bl&0x01u)btn|=SCE_PAD_BUTTON_DOWN;
    if(bl&0x04u)btn|=SCE_PAD_BUTTON_RIGHT;if(bl&0x08u)btn|=SCE_PAD_BUTTON_LEFT;
    o->buttons=btn;o->leftStick.x=scale12(lx);o->leftStick.y=scale12(ly);
    o->rightStick.x=scale12(rx);o->rightStick.y=scale12(ry);
    o->analogButtons.l2=(bl&0x80u)?255u:0u;o->analogButtons.r2=(br&0x80u)?255u:0u;return 1;
}

static int parse_switch_simple(const uint8_t *b,uint32_t l,ScePadData *o){
    if(l<9||b[0]!=0x3f)return 0;
    uint8_t b1=b[1],b2=b[2],hat=b[3],b8=b[8];uint32_t btn=hat_to_dpad(hat);
    if(b1&0x04u)btn|=SCE_PAD_BUTTON_CROSS;if(b1&0x08u)btn|=SCE_PAD_BUTTON_CIRCLE;
    if(b1&0x01u)btn|=SCE_PAD_BUTTON_SQUARE;if(b1&0x02u)btn|=SCE_PAD_BUTTON_TRIANGLE;
    if(b8&0x40u)btn|=SCE_PAD_BUTTON_L1;if(b8&0x80u)btn|=SCE_PAD_BUTTON_L2;
    if(b1&0x40u)btn|=SCE_PAD_BUTTON_R1;if(b1&0x80u)btn|=SCE_PAD_BUTTON_R2;
    if(b2&0x08u)btn|=SCE_PAD_BUTTON_L3;if(b2&0x04u)btn|=SCE_PAD_BUTTON_R3;
    if(b2&0x02u)btn|=SCE_PAD_BUTTON_OPTIONS;if(b2&0x01u)btn|=SCE_PAD_BUTTON_CREATE;
    if(b2&0x10u)btn|=SCE_PAD_BUTTON_PS;if(b2&0x20u)btn|=SCE_PAD_BUTTON_TOUCH_PAD;
    o->buttons=btn;o->leftStick.x=b[4];o->leftStick.y=b[5];
    o->rightStick.x=b[6];o->rightStick.y=b[7];
    o->analogButtons.l2=(b8&0x80u)?255u:0u;o->analogButtons.r2=(b1&0x80u)?255u:0u;return 1;
}

static int parse_compact_hid(const uint8_t *b,uint32_t l,ScePadData *o,int *hs){
    uint8_t off=maybe_off(b,l);uint32_t btn=0;uint16_t bits=0;
    if(l<(uint32_t)off+6)return 0;
    o->leftStick.x=b[off+0];o->leftStick.y=b[off+1];o->rightStick.x=b[off+2];o->rightStick.y=b[off+3];
    if(l>=(uint32_t)off+9&&(b[off+6]&0xf)<=8){
        o->analogButtons.l2=b[off+4];o->analogButtons.r2=b[off+5];
        btn|=hat_to_dpad(b[off+6]);bits=(uint16_t)b[off+7]|((uint16_t)b[off+8]<<8);
        *hs=GEN_FMT_HID_6A_HAT_BTN;
    }else if(l>=(uint32_t)off+7&&(b[off+4]&0xf)<=8){
        btn|=hat_to_dpad(b[off+4]);bits=(uint16_t)b[off+5]|((uint16_t)b[off+6]<<8);
        *hs=GEN_FMT_HID_4A_HAT_BTN;
    }else{
        bits=(uint16_t)b[off+4];if(l>(uint32_t)off+5)bits|=(uint16_t)b[off+5]<<8;
        *hs=GEN_FMT_HID_4A_BTN;
    }
    btn|=map_button_bits(bits);
    if(o->analogButtons.l2>16u)btn|=SCE_PAD_BUTTON_L2;
    if(o->analogButtons.r2>16u)btn|=SCE_PAD_BUTTON_R2;
    o->buttons=btn;return 1;
}

static int generic_parse(int fd, struct usb_fs_endpoint eps[4],
                         const uint8_t *buf, uint32_t len,
                         ScePadData *out, int *hs, uint8_t *seq)
{
    (void)fd;(void)eps;(void)seq;
    if(len==0)return 0;
    pad_defaults(out);
    if(parse_ds4_like(buf,len,out)){if(*hs!=GEN_FMT_DS4_LIKE){KLOG("Generic: DS4-like\n");*hs=GEN_FMT_DS4_LIKE;}return 1;}
    if(parse_switch_full(buf,len,out)){if(*hs!=GEN_FMT_SWITCH_FULL){KLOG("Generic: Switch-full\n");*hs=GEN_FMT_SWITCH_FULL;}return 1;}
    if(parse_switch_simple(buf,len,out)){if(*hs!=GEN_FMT_SWITCH_SIMPLE){KLOG("Generic: Switch-simple\n");*hs=GEN_FMT_SWITCH_SIMPLE;}return 1;}
    if(parse_compact_hid(buf,len,out,hs))return 1;
    return 0;
}

static int generic_init(const char *dev_path, int *fd_out,
                        struct usb_fs_endpoint eps[4],
                        int *hs_out, uint8_t *seq_out)
{
    int fd; struct usb_fs_init init; struct usb_fs_open fs_open;
    *hs_out=GEN_FMT_UNKNOWN;*seq_out=0;
    fd=open(dev_path,O_RDWR);if(fd<0){KLOG("Generic open fail\n");return -1;}
    for(int iface=0;iface<4;iface++){int i=iface;ioctl(fd,USB_IFACE_DRIVER_DETACH,&i);}
    usleep(100000);
    memset(eps,0,sizeof(eps[0])*4);
    memset(&init,0,sizeof(init));init.pEndpoints=eps;init.ep_index_max=1;
    if(ioctl(fd,USB_FS_INIT,&init)!=0){KLOG("Generic FS_INIT fail\n");close(fd);return -1;}
    for(uint8_t ep=GENERIC_EP_FIRST_IN;ep<=GENERIC_EP_LAST_IN;ep++){
        memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=0;fs_open.ep_no=ep;
        fs_open.max_bufsize=CTRL_REPORT_MAX;fs_open.max_frames=1;
        if(ioctl(fd,USB_FS_OPEN,&fs_open)==0){KLOG("Generic IN ep=0x%02x\n",ep);*fd_out=fd;return 0;}
    }
    KLOG("Generic: no IN endpoint\n");
    {struct usb_fs_uninit u;memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u);}
    close(fd);return -1;
}

static void generic_deinit(int fd, struct usb_fs_endpoint eps[4])
{
    (void)eps;
    struct usb_fs_stop sp;struct usb_fs_close fc;struct usb_fs_uninit u;
    memset(&sp,0,sizeof(sp));sp.ep_index=0;ioctl(fd,USB_FS_STOP,&sp);
    memset(&fc,0,sizeof(fc));fc.ep_index=0;ioctl(fd,USB_FS_CLOSE,&fc);
    memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u);close(fd);
}

const CtrlOps g_ctrl_generic_ops = {
    .name="Generic HID", .init=generic_init, .parse=generic_parse, .deinit=generic_deinit,
};
