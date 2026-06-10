/* SPDX-License-Identifier: GPL-3.0-or-later
 * xbox.c — Xbox One / Series GIP controller adapter
 */

#include "controller.h"
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

/* GIP endpoint addresses & command bytes */
#define XBOX_EP_IN   0x82
#define XBOX_EP_OUT  0x02
#define GIP_CMD_ACK      0x01
#define GIP_CMD_ANNOUNCE 0x02
#define GIP_CMD_INPUT    0x20

#define DEADZONE 7849
static uint32_t g_xbox_input_count = 0;

static uint8_t trig_scale(uint16_t v) {
    if (v>1023u) v=1023u; return (uint8_t)((v*255u)/1023u);
}
static uint8_t stick_x(int16_t v) {
    return (v>DEADZONE||v<-DEADZONE)?(uint8_t)((v+32768)>>8):128u;
}
static uint8_t stick_y(int16_t v) {
    return (v>DEADZONE||v<-DEADZONE)?(uint8_t)(255-((v+32768)>>8)):128u;
}

static int xbox_parse(int fd, struct usb_fs_endpoint eps[4],
                      const uint8_t *buf, uint32_t len,
                      ScePadData *out, int *hs, uint8_t *seq)
{
    (void)hs;(void)seq;
    uint8_t cmd = buf[0];

    if (cmd == GIP_CMD_INPUT && len >= 18) {
        g_xbox_input_count++;
        uint8_t b4=buf[4], b5=buf[5];
        uint16_t lt16=(uint16_t)buf[6]|((uint16_t)buf[7]<<8);
        uint16_t rt16=(uint16_t)buf[8]|((uint16_t)buf[9]<<8);
        int16_t lx=(int16_t)((uint16_t)buf[10]|((uint16_t)buf[11]<<8));
        int16_t ly=(int16_t)((uint16_t)buf[12]|((uint16_t)buf[13]<<8));
        int16_t rx=(int16_t)((uint16_t)buf[14]|((uint16_t)buf[15]<<8));
        int16_t ry=(int16_t)((uint16_t)buf[16]|((uint16_t)buf[17]<<8));
        uint8_t lt=trig_scale(lt16), rt=trig_scale(rt16);
        out->leftStick.x=stick_x(lx); out->leftStick.y=stick_y(ly);
        out->rightStick.x=stick_x(rx); out->rightStick.y=stick_y(ry);
        out->analogButtons.l2=lt; out->analogButtons.r2=rt;
        uint32_t btn=0;
        if(b4&0x04u)btn|=SCE_PAD_BUTTON_OPTIONS; if(b4&0x08u)btn|=SCE_PAD_BUTTON_SHARE;
        if(b4&0x10u)btn|=SCE_PAD_BUTTON_CROSS; if(b4&0x20u)btn|=SCE_PAD_BUTTON_CIRCLE;
        if(b4&0x40u)btn|=SCE_PAD_BUTTON_SQUARE; if(b4&0x80u)btn|=SCE_PAD_BUTTON_TRIANGLE;
        if(b5&0x01u)btn|=SCE_PAD_BUTTON_UP; if(b5&0x02u)btn|=SCE_PAD_BUTTON_DOWN;
        if(b5&0x04u)btn|=SCE_PAD_BUTTON_LEFT; if(b5&0x08u)btn|=SCE_PAD_BUTTON_RIGHT;
        if(b5&0x10u)btn|=SCE_PAD_BUTTON_L1; if(b5&0x20u)btn|=SCE_PAD_BUTTON_R1;
        if(b5&0x40u)btn|=SCE_PAD_BUTTON_L3; if(b5&0x80u)btn|=SCE_PAD_BUTTON_R3;
        if(lt>16u)btn|=SCE_PAD_BUTTON_L2; if(rt>16u)btn|=SCE_PAD_BUTTON_R2;
        out->buttons=btn;
        return 1;
    }
    if (cmd == 0x07) {
        out->leftStick.x=128;out->leftStick.y=128;out->rightStick.x=128;out->rightStick.y=128;
        if(g_xbox_input_count>10 && len>=5 && (buf[4]&0x01u))
            out->buttons=SCE_PAD_BUTTON_PS;
        return 1;
    }
    if (cmd == GIP_CMD_ANNOUNCE && len >= 4) {
        uint8_t ack[8]={GIP_CMD_ACK,0,0,4,buf[1],GIP_CMD_ANNOUNCE,0,0};
        static const uint8_t pw[]={0x05,0x20,0,1,0};
        usb_send_out(fd,&eps[1],ack,8,"ack");
        usb_send_out(fd,&eps[1],pw,5,"repower");
        return 0;
    }
    return 0;
}

static int xbox_init(const char *dev_path, int *fd_out,
                     struct usb_fs_endpoint eps[4],
                     int *hs_out, uint8_t *seq_out)
{
    int fd, ii; struct usb_fs_init init; struct usb_fs_open fs_open;
    *hs_out=HS_STREAMING; *seq_out=0; g_xbox_input_count=0;
    fd=open(dev_path,O_RDWR);
    if(fd<0){KLOG("Xbox open fail\n");return -1;}
    for(ii=0;ii<4;ii++){int iface=ii;ioctl(fd,USB_IFACE_DRIVER_DETACH,&iface);}
    usleep(120000);
    memset(eps,0,sizeof(eps[0])*4);
    memset(&init,0,sizeof(init));init.pEndpoints=eps;init.ep_index_max=4;
    if(ioctl(fd,USB_FS_INIT,&init)!=0){KLOG("Xbox FS_INIT fail\n");close(fd);return -1;}

    memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=0;
    fs_open.ep_no=XBOX_EP_IN;fs_open.max_bufsize=64;fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){KLOG("Xbox IN fail\n");goto fail;}
    KLOG("Xbox IN ok maxpkt=%u\n",(unsigned)fs_open.max_packet_length);

    memset(&fs_open,0,sizeof(fs_open));fs_open.ep_index=1;
    fs_open.ep_no=XBOX_EP_OUT;fs_open.max_bufsize=64;fs_open.max_frames=1;
    if(ioctl(fd,USB_FS_OPEN,&fs_open)!=0){KLOG("Xbox OUT fail\n");goto fail;}

    { /* GIP handshake */
        static const uint8_t pw[]={0x05,0x20,0,1,0};
        static const uint8_t hello[]={GIP_CMD_ACK,0,0,0};
        uint8_t rbuf[64]; int announced=0;
        for(int at=0;at<4&&!announced;at++){
            usb_send_out(fd,&eps[1],pw,5,"power");
            if(at>0) usb_send_out(fd,&eps[1],hello,4,"hello");
            for(int i=0;i<30&&!announced;i++){
                void *pb[1]={rbuf}; uint32_t pl[1]={64};
                eps[0].ppBuffer=pb;eps[0].pLength=pl;eps[0].nFrames=1;
                eps[0].timeout=100;eps[0].aFrames=0;eps[0].status=0;
                eps[0].flags=USB_FS_FLAG_SINGLE_SHORT_OK|USB_FS_FLAG_MULTI_SHORT_OK;
                struct usb_fs_start st;memset(&st,0,sizeof(st));st.ep_index=0;
                if(ioctl(fd,USB_FS_START,&st)!=0)continue;
                for(int w=0;w<5;w++){
                    struct usb_fs_complete co;memset(&co,0,sizeof(co));co.ep_index=0;
                    if(ioctl(fd,USB_FS_COMPLETE,&co)==0){
                        int rn=(int)pl[0];
                        if(rn>0&&rbuf[0]==GIP_CMD_ANNOUNCE){
                            uint8_t ak[8]={GIP_CMD_ACK,0,0,4,rbuf[1],GIP_CMD_ANNOUNCE,0,0};
                            usb_send_out(fd,&eps[1],ak,8,"ack");announced=1;
                        }else if(rn>0&&rbuf[0]==GIP_CMD_INPUT)announced=1;
                        break;
                    }
                    if(errno!=EBUSY)break;
                    usleep(50000);
                }
                { struct usb_fs_stop sp;memset(&sp,0,sizeof(sp));sp.ep_index=0;ioctl(fd,USB_FS_STOP,&sp); }
            }
        }
        KLOG("Xbox handshake announced=%d\n",announced);
    }
    *fd_out=fd; return 0;
fail:
    { struct usb_fs_uninit u;memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u); }
    close(fd); return -1;
}

static void xbox_deinit(int fd, struct usb_fs_endpoint eps[4])
{
    struct usb_fs_stop sp;struct usb_fs_close fc;struct usb_fs_uninit u;
    memset(&sp,0,sizeof(sp));sp.ep_index=0;ioctl(fd,USB_FS_STOP,&sp);
    memset(&sp,0,sizeof(sp));sp.ep_index=1;ioctl(fd,USB_FS_STOP,&sp);
    memset(&fc,0,sizeof(fc));fc.ep_index=0;ioctl(fd,USB_FS_CLOSE,&fc);
    memset(&fc,0,sizeof(fc));fc.ep_index=1;ioctl(fd,USB_FS_CLOSE,&fc);
    memset(&u,0,sizeof(u));ioctl(fd,USB_FS_UNINIT,&u);close(fd);
}

const CtrlOps g_ctrl_xbox_ops = {
    .name="Xbox One/Series", .init=xbox_init, .parse=xbox_parse, .deinit=xbox_deinit,
};
