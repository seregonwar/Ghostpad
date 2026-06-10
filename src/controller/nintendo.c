/* SPDX-License-Identifier: GPL-3.0-or-later
 * nintendo.c — Nintendo Switch Pro / 8BitDo adapter
 */

#include "controller.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* log via controller.h KLOG macro, helpers via controller.h inlines */

static int nintendo_send_subcmd(int fd, struct usb_fs_endpoint *ep,
                                uint8_t *seq, uint8_t sc,
                                const uint8_t *data, uint32_t dlen)
{
    static const uint8_t rumble[8]={0,1,0x40,0x40,0,1,0x40,0x40};
    uint8_t buf[64]; uint32_t total=11+dlen;
    if(total>sizeof(buf))return -1;
    memset(buf,0,sizeof(buf)); buf[0]=1; buf[1]=*seq&0xf;
    memcpy(buf+2,rumble,8); buf[10]=sc;
    if(dlen)memcpy(buf+11,data,dlen);
    *seq=(uint8_t)((*seq+1)&0xf);
    char tag[16]; snprintf(tag,sizeof(tag),"sc%02x",sc);
    return usb_send_out(fd,ep,buf,total,tag);
}

static int nintendo_parse(int fd, struct usb_fs_endpoint eps[4],
                          const uint8_t *buf, uint32_t len,
                          ScePadData *out, int *hs, uint8_t *seq)
{
    uint8_t rid=buf[0];

    if((rid==0x00||rid==0x30)&&len>=12){
        if(rid==0x00&&buf[1]==0)return 0;
        if(*hs!=HS_STREAMING)*hs=HS_STREAMING;
        uint8_t br=buf[3],bs=buf[4],bl=buf[5];
        uint16_t lx=(uint16_t)(buf[6]|((buf[7]&0xF)<<8));
        uint16_t ly=(uint16_t)((buf[7]>>4)|((uint16_t)buf[8]<<4));
        uint16_t rx=(uint16_t)(buf[9]|((buf[10]&0xF)<<8));
        uint16_t ry=(uint16_t)((buf[10]>>4)|((uint16_t)buf[11]<<4));
        uint32_t btn=0;
        if(br&0x04)btn|=SCE_PAD_BUTTON_CROSS;if(br&0x08)btn|=SCE_PAD_BUTTON_CIRCLE;
        if(br&0x01)btn|=SCE_PAD_BUTTON_SQUARE;if(br&0x02)btn|=SCE_PAD_BUTTON_TRIANGLE;
        if(bl&0x40)btn|=SCE_PAD_BUTTON_L1;if(bl&0x80)btn|=SCE_PAD_BUTTON_L2;
        if(br&0x40)btn|=SCE_PAD_BUTTON_R1;if(br&0x80)btn|=SCE_PAD_BUTTON_R2;
        if(bs&0x08)btn|=SCE_PAD_BUTTON_L3;if(bs&0x04)btn|=SCE_PAD_BUTTON_R3;
        if(bs&0x02)btn|=SCE_PAD_BUTTON_OPTIONS;if(bs&0x01)btn|=SCE_PAD_BUTTON_CREATE;
        if(bs&0x10)btn|=SCE_PAD_BUTTON_PS;if(bs&0x20)btn|=SCE_PAD_BUTTON_TOUCH_PAD;
        if(bl&0x02)btn|=SCE_PAD_BUTTON_UP;if(bl&0x01)btn|=SCE_PAD_BUTTON_DOWN;
        if(bl&0x04)btn|=SCE_PAD_BUTTON_RIGHT;if(bl&0x08)btn|=SCE_PAD_BUTTON_LEFT;
        out->buttons=btn; out->leftStick.x=stick_scale_12to8(lx); out->leftStick.y=stick_scale_12to8(ly);
        out->rightStick.x=stick_scale_12to8(rx); out->rightStick.y=stick_scale_12to8(ry);
        out->analogButtons.l2=(bl&0x80)?255:0; out->analogButtons.r2=(br&0x80)?255:0;
        return 1;
    }
    if(rid==0x21&&len>=12){
        if(*hs==HS_STREAMING) return nintendo_parse(fd,eps,buf,len,out,hs,seq);
        return 0;
    }
    if(rid==0x3f&&len>=9){
        uint8_t b1=buf[1],b2=buf[2],hat=buf[3],b8=buf[8];
        uint32_t btn=hat_to_dpad(hat);
        if(b1&0x04)btn|=SCE_PAD_BUTTON_CROSS;if(b1&0x08)btn|=SCE_PAD_BUTTON_CIRCLE;
        if(b1&0x01)btn|=SCE_PAD_BUTTON_SQUARE;if(b1&0x02)btn|=SCE_PAD_BUTTON_TRIANGLE;
        if(b8&0x40)btn|=SCE_PAD_BUTTON_L1;if(b8&0x80)btn|=SCE_PAD_BUTTON_L2;
        if(b1&0x40)btn|=SCE_PAD_BUTTON_R1;if(b1&0x80)btn|=SCE_PAD_BUTTON_R2;
        if(b2&0x08)btn|=SCE_PAD_BUTTON_L3;if(b2&0x04)btn|=SCE_PAD_BUTTON_R3;
        if(b2&0x02)btn|=SCE_PAD_BUTTON_OPTIONS;if(b2&0x01)btn|=SCE_PAD_BUTTON_CREATE;
        if(b2&0x10)btn|=SCE_PAD_BUTTON_PS;if(b2&0x20)btn|=SCE_PAD_BUTTON_TOUCH_PAD;
        out->buttons=btn; out->leftStick.x=buf[4];out->leftStick.y=buf[5];
        out->rightStick.x=buf[6];out->rightStick.y=buf[7];
        out->analogButtons.l2=(b8&0x80)?255:0; out->analogButtons.r2=(b1&0x80)?255:0;
        return 1;
    }
    if(rid==0x81){
        if(*hs==HS_STREAMING){*hs=HS_WAIT_81_01;return 0;}
        if(buf[1]==0x01&&*hs==HS_WAIT_81_01){usb_send_cmd(fd,&eps[1],0x80,0x02);*hs=HS_WAIT_81_02;}
        else if(buf[1]==0x02&&*hs<=HS_WAIT_81_02){
            usb_send_cmd(fd,&eps[1],0x80,0x04);
            uint8_t d[]={0x01};
            nintendo_send_subcmd(fd,&eps[1],seq,0x40,d,1);
            nintendo_send_subcmd(fd,&eps[1],seq,0x48,d,1);
            nintendo_send_subcmd(fd,&eps[1],seq,0x30,d,1);
            uint8_t d2[]={0x30}; nintendo_send_subcmd(fd,&eps[1],seq,0x03,d2,1);
            *hs=HS_STREAMING;
        }
        return 0;
    }
    return 0;
}

static int nintendo_init(const char *dev_path, int *fd_out,
                         struct usb_fs_endpoint eps[4], int *hs_out, uint8_t *seq_out)
{
    int fd=-1, dt; struct usb_fs_uninit uninit;
    *hs_out=HS_WAIT_81_01; *seq_out=0;

    /* Pass 1: confirm device */
    fd=open(dev_path,O_RDWR); if(fd<0)return -1;
    memset(eps,0,sizeof(eps[0])*4);
    struct usb_fs_init init; memset(&init,0,sizeof(init)); init.pEndpoints=eps; init.ep_index_max=1;
    if(ioctl(fd,USB_FS_INIT,&init)!=0){close(fd);return -1;}
    {int i0=0,i1=1;ioctl(fd,USB_IFACE_DRIVER_DETACH,&i0);ioctl(fd,USB_IFACE_DRIVER_DETACH,&i1);}
    struct usb_fs_open fs_open; memset(&fs_open,0,sizeof(fs_open));
    fs_open.ep_index=0;fs_open.ep_no=0x81;fs_open.max_bufsize=64;fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){goto fail1;}
    memset(&uninit,0,sizeof(uninit));ioctl(fd,USB_FS_UNINIT,&uninit);close(fd);fd=-1;

    /* Pass 2: beat HID re-attach, open IN+OUT */
    for(dt=0;dt<5;dt++){
        fd=open(dev_path,O_RDWR); if(fd<0)return -1;
        {int i0=0,i1=1,i2=2;ioctl(fd,USB_IFACE_DRIVER_DETACH,&i0);ioctl(fd,USB_IFACE_DRIVER_DETACH,&i1);ioctl(fd,USB_IFACE_DRIVER_DETACH,&i2);}
        memset(eps,0,sizeof(eps[0])*4); memset(&init,0,sizeof(init)); init.pEndpoints=eps;init.ep_index_max=2;
        if(ioctl(fd,USB_FS_INIT,&init)!=0){close(fd);fd=-1;usleep(50000);continue;}
        memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=0;fs_open.ep_no=0x81;fs_open.max_bufsize=64;fs_open.max_frames=1;
        if(ioctl(fd,USB_FS_OPEN,&fs_open)==0)break;
        memset(&uninit,0,sizeof(uninit));ioctl(fd,USB_FS_UNINIT,&uninit);close(fd);fd=-1;usleep(50000);
    }
    if(fd<0)return -1;

    /* OUT: try 0x02 (8BitDo), then 0x01 (real Switch Pro) */
    memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=1;fs_open.ep_no=0x02;fs_open.max_bufsize=64;fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){
        memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=1;fs_open.ep_no=0x01;fs_open.max_bufsize=64;fs_open.max_frames=1;
        ioctl(fd,USB_FS_OPEN,&fs_open);
    }
    usb_send_cmd(fd,&eps[1],0x80,0x02);usleep(30000);
    usb_send_cmd(fd,&eps[1],0x80,0x04);usleep(50000);
    *hs_out=HS_WAIT_81_02; *fd_out=fd; return 0;
fail1:
    {struct usb_fs_uninit u;memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u);}close(fd);return -1;
}

static void nintendo_deinit(int fd, struct usb_fs_endpoint eps[4]) { usb_dev_close(fd, eps, 2); }

const CtrlOps g_ctrl_nintendo_ops = { "Nintendo", nintendo_init, nintendo_parse, nintendo_deinit };
