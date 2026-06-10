/* SPDX-License-Identifier: GPL-3.0-or-later
 * xbox.c — Xbox One / Series GIP controller adapter
 */

#include "controller.h"
#include <string.h>
#include <errno.h>

/* GIP constants */
#define XBOX_EP_IN   0x82
#define XBOX_EP_OUT  0x02
#define GIP_ACK      0x01
#define GIP_ANNOUNCE 0x02
#define GIP_INPUT    0x20
#define DEADZONE 7849

static uint32_t g_xbox_count = 0;

static uint8_t trig(uint16_t v) { if(v>1023u)v=1023u; return (uint8_t)((v*255u)/1023u); }
static uint8_t stk_x(int16_t v) { return (v>DEADZONE||v<-DEADZONE)?(uint8_t)((v+32768)>>8):128u; }
static uint8_t stk_y(int16_t v) { return (v>DEADZONE||v<-DEADZONE)?(uint8_t)(255-((v+32768)>>8)):128u; }

static int xbox_parse(int fd, struct usb_fs_endpoint eps[4],
                      const uint8_t *buf, uint32_t len,
                      ScePadData *out, int *hs, uint8_t *seq)
{
    (void)hs;(void)seq;
    uint8_t cmd = buf[0];

    if (cmd == GIP_INPUT && len >= 18) {
        g_xbox_count++;
        uint8_t b4=buf[4], b5=buf[5];
        uint16_t lt16=(uint16_t)buf[6]|((uint16_t)buf[7]<<8), rt16=(uint16_t)buf[8]|((uint16_t)buf[9]<<8);
        int16_t lx=(int16_t)((uint16_t)buf[10]|((uint16_t)buf[11]<<8)), ly=(int16_t)((uint16_t)buf[12]|((uint16_t)buf[13]<<8));
        int16_t rx=(int16_t)((uint16_t)buf[14]|((uint16_t)buf[15]<<8)), ry=(int16_t)((uint16_t)buf[16]|((uint16_t)buf[17]<<8));
        uint8_t lt=trig(lt16), rt=trig(rt16);
        out->leftStick.x=stk_x(lx);out->leftStick.y=stk_y(ly);out->rightStick.x=stk_x(rx);out->rightStick.y=stk_y(ry);
        out->analogButtons.l2=lt;out->analogButtons.r2=rt;
        uint32_t btn=0;
        if(b4&0x04u)btn|=SCE_PAD_BUTTON_OPTIONS;if(b4&0x08u)btn|=SCE_PAD_BUTTON_SHARE;
        if(b4&0x10u)btn|=SCE_PAD_BUTTON_CROSS;if(b4&0x20u)btn|=SCE_PAD_BUTTON_CIRCLE;
        if(b4&0x40u)btn|=SCE_PAD_BUTTON_SQUARE;if(b4&0x80u)btn|=SCE_PAD_BUTTON_TRIANGLE;
        if(b5&0x01u)btn|=SCE_PAD_BUTTON_UP;if(b5&0x02u)btn|=SCE_PAD_BUTTON_DOWN;
        if(b5&0x04u)btn|=SCE_PAD_BUTTON_LEFT;if(b5&0x08u)btn|=SCE_PAD_BUTTON_RIGHT;
        if(b5&0x10u)btn|=SCE_PAD_BUTTON_L1;if(b5&0x20u)btn|=SCE_PAD_BUTTON_R1;
        if(b5&0x40u)btn|=SCE_PAD_BUTTON_L3;if(b5&0x80u)btn|=SCE_PAD_BUTTON_R3;
        if(lt>16u)btn|=SCE_PAD_BUTTON_L2;if(rt>16u)btn|=SCE_PAD_BUTTON_R2;
        out->buttons=btn; return 1;
    }
    if (cmd == 0x07) {
        out->leftStick.x=128;out->leftStick.y=128;out->rightStick.x=128;out->rightStick.y=128;
        if(g_xbox_count>10 && len>=5 && (buf[4]&0x01u)) out->buttons=SCE_PAD_BUTTON_PS;
        return 1;
    }
    if (cmd == GIP_ANNOUNCE && len >= 4) {
        uint8_t ack[8]={GIP_ACK,0,0,4,buf[1],GIP_ANNOUNCE,0,0};
        static const uint8_t pw[]={0x05,0x20,0,1,0};
        usb_send_out(fd,&eps[1],ack,8,"ack"); usb_send_out(fd,&eps[1],pw,5,"repower");
        return 0;
    }
    return 0;
}

static int xbox_init(const char *dev_path, int *fd_out,
                     struct usb_fs_endpoint eps[4], int *hs_out, uint8_t *seq_out)
{
    *hs_out=HS_STREAMING; *seq_out=0; g_xbox_count=0;
    int fd = usb_dev_open(dev_path, eps, 4);
    if (fd < 0) { KLOG("Xbox open fail\n"); return -1; }
    usleep(20000); /* extra settle after detach */

    if (usb_ep_open(fd, eps, 0, XBOX_EP_IN, 64) != 0 ||
        usb_ep_open(fd, eps, 1, XBOX_EP_OUT, 64) != 0) {
        KLOG("Xbox EP fail\n"); usb_dev_close(fd, eps, 0); return -1;
    }

    /* GIP handshake: POWER ON -> wait ANNOUNCE -> ACK */
    static const uint8_t pw[]={0x05,0x20,0,1,0};
    static const uint8_t hello[]={GIP_ACK,0,0,0};
    uint8_t rbuf[64]; int announced=0;
    for (int at=0; at<4 && !announced; at++) {
        usb_send_out(fd,&eps[1],pw,5,"power");
        if(at>0) usb_send_out(fd,&eps[1],hello,4,"hello");
        for (int i=0; i<30 && !announced; i++) {
            void *pb[1]={rbuf}; uint32_t pl[1]={64};
            eps[0].ppBuffer=pb;eps[0].pLength=pl;eps[0].nFrames=1;
            eps[0].timeout=100;eps[0].aFrames=0;eps[0].status=0;
            eps[0].flags=USB_FS_FLAG_SINGLE_SHORT_OK|USB_FS_FLAG_MULTI_SHORT_OK;
            struct usb_fs_start st;memset(&st,0,sizeof(st));st.ep_index=0;
            if(ioctl(fd,USB_FS_START,&st)!=0)continue;
            for(int w=0;w<5;w++){
                struct usb_fs_complete co;memset(&co,0,sizeof(co));co.ep_index=0;
                if(ioctl(fd,USB_FS_COMPLETE,&co)==0){
                    if(pl[0]>0 && rbuf[0]==GIP_ANNOUNCE){
                        uint8_t ack[8]={GIP_ACK,0,0,4,rbuf[1],GIP_ANNOUNCE,0,0};
                        usb_send_out(fd,&eps[1],ack,8,"ack"); announced=1;
                    }else if(pl[0]>0 && rbuf[0]==GIP_INPUT) announced=1;
                    break;
                }
                if(errno!=EBUSY)break; usleep(50000);
            }
            { struct usb_fs_stop sp;memset(&sp,0,sizeof(sp));sp.ep_index=0;ioctl(fd,USB_FS_STOP,&sp); }
        }
    }
    *fd_out=fd; return 0;
}

static void xbox_deinit(int fd, struct usb_fs_endpoint eps[4]) { usb_dev_close(fd, eps, 2); }

const CtrlOps g_ctrl_xbox_ops = { "Xbox", xbox_init, xbox_parse, xbox_deinit };
