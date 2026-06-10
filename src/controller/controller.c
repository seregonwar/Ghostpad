/* SPDX-License-Identifier: GPL-3.0-or-later
 * controller.c — USB controller registry, probe, generic read/inject loop,
 *                and USB send helpers.
 */

#include "controller.h"
#include "ctrl_registry.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <dev/usb/usb_endian.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#define KLOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define KLOG(...) fprintf(stderr, __VA_ARGS__)
#endif

/* ── Per-controller ops (defined in each controller .c) ──────────────── */
extern const CtrlOps g_ctrl_ds4_ops;
extern const CtrlOps g_ctrl_xbox_ops;
extern const CtrlOps g_ctrl_nintendo_ops;
extern const CtrlOps g_ctrl_generic_ops;
extern const CtrlOps g_ctrl_logitech_ops;

/* ── USB helpers ──────────────────────────────────────────────────────── */

int usb_send_out(int fd, struct usb_fs_endpoint *ep,
                 const uint8_t *data, uint32_t len, const char *tag)
{
    void    *bufs[1]  = { (void *)data };
    uint32_t lens[1]  = { len };
    struct usb_fs_start    start;
    struct usb_fs_complete complete;

    ep->ppBuffer = bufs;   ep->pLength  = lens;
    ep->nFrames  = 1;      ep->timeout  = 150;
    ep->flags    = 0;      ep->aFrames  = 0;  ep->status = 0;

    memset(&start, 0, sizeof(start)); start.ep_index = 1;
    if (ioctl(fd, USB_FS_START, &start) != 0) {
        KLOG("OUT %s START fail errno=%d\n", tag, errno);
        return -errno;
    }
    for (int w = 0; w < 20; w++) {
        memset(&complete, 0, sizeof(complete)); complete.ep_index = 1;
        if (ioctl(fd, USB_FS_COMPLETE, &complete) == 0) return 0;
        if (errno != EBUSY) { KLOG("OUT %s fail errno=%d\n", tag, errno); return -errno; }
        usleep(50000);
    }
    KLOG("OUT %s timeout\n", tag);
    return -EBUSY;
}

int usb_send_cmd(int fd, struct usb_fs_endpoint *ep, uint8_t a, uint8_t b)
{
    uint8_t buf[2] = { a, b };
    char tag[8]; snprintf(tag, sizeof(tag), "%02x%02x", a, b);
    return usb_send_out(fd, ep, buf, 2, tag);
}

/* ── Registry ─────────────────────────────────────────────────────────── */

static const CtrlDesc g_registry[] = {
    { VID_SONY,   0x0000, "DualShock 4 / DS4-compatible", &g_ctrl_ds4_ops, 64u, 0, 0 },
    { VID_HORI,   0x0000, "HORIPAD / DS4-compatible",      &g_ctrl_ds4_ops, 64u, 0, 0 },
    { VID_XBOX,   PID_XBOX, "Xbox One / Series",           &g_ctrl_xbox_ops, 64u, 0, 0 },
    { VID_SWITCH, PID_SWITCH, "Nintendo Switch Pro / 8BitDo", &g_ctrl_nintendo_ops, 64u, 0, 0 },
    { VID_LOGITECH, 0x0000, "Logitech HID Gamepad", &g_ctrl_logitech_ops, 64u, 4,
      CTRL_FLAG_CACHED_STATE | CTRL_FLAG_PER_READ_STOP | CTRL_FLAG_POLL_READ },
};
#define REGISTRY_SIZE (sizeof(g_registry) / sizeof(g_registry[0]))

static const CtrlDesc g_generic_hid_desc = {
    0x0000, 0x0000, "Generic HID Gamepad", &g_ctrl_generic_ops, CTRL_REPORT_MAX, 0, 0
};

static int ctrl_is_external_ugen(const char *path)
{
    const char *p = strstr(path, "/dev/ugen"); char *end = NULL; long bus;
    if (p) p += strlen("/dev/ugen");
    else if (strncmp(path, "ugen", 4) == 0) p = path + 4;
    else return 0;
    bus = strtol(p, &end, 10);
    return end && *end == '.' && bus >= 2;
}

static int ctrl_should_skip_unknown(uint8_t dev_cls, uint8_t if_cls,
                                    uint8_t if_sub, uint8_t if_proto)
{
    if (dev_cls == 0x09 || if_cls == 0x09) return 1;
    if (dev_cls == 0x08 || if_cls == 0x08) return 1;
    if (dev_cls == 0xef || if_cls == 0xe0) return 1;
    if (if_cls == 0x03 && if_sub == 0x01 && (if_proto == 0x01 || if_proto == 0x02))
        return 1;
    return 0;
}

const CtrlDesc *ctrl_lookup(uint16_t vid, uint16_t pid)
{
    for (size_t i = 0; i < REGISTRY_SIZE; i++) {
        const CtrlDesc *d = &g_registry[i];
        if (d->vid == vid && (d->pid == pid || d->pid == 0x0000)) return d;
    }
    if (ds4_is_compatible_vidpid(vid, pid))
        return &g_registry[0];
    return NULL;
}

const CtrlDesc *ctrl_probe(const char *path, uint16_t *out_vid, uint16_t *out_pid)
{
    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) return NULL;
    *out_vid = 0; *out_pid = 0;

    struct usb_device_descriptor desc;
    memset(&desc, 0, sizeof(desc));
    if (ioctl(fd, USB_GET_DEVICE_DESC, &desc) == 0) {
        uint16_t vid = UGETW(desc.idVendor);
        uint16_t pid = UGETW(desc.idProduct);
        KLOG("scan: %s VID=0x%04x PID=0x%04x\n", path, vid, pid);

        struct usb_interface_descriptor iface;
        memset(&iface, 0, sizeof(iface));
        if (ioctl(fd, USB_GET_RX_INTERFACE_DESC, &iface) == 0 &&
            iface.bInterfaceSubClass == XBOX_GIP_SUBCLASS &&
            iface.bInterfaceProtocol == XBOX_GIP_PROTOCOL) {
            KLOG("scan: %s GIP Xbox (sub=0x%02x proto=0x%02x)\n",
                 path, iface.bInterfaceSubClass, iface.bInterfaceProtocol);
            *out_vid = VID_XBOX; *out_pid = PID_XBOX;
            close(fd);
            return ctrl_lookup(VID_XBOX, PID_XBOX);
        }

        const CtrlDesc *found = ctrl_lookup(vid, pid);
        if (found) { *out_vid = vid; *out_pid = pid; close(fd); return found; }

        if (ctrl_is_external_ugen(path) &&
            !ctrl_should_skip_unknown(desc.bDeviceClass, iface.bInterfaceClass,
                                      iface.bInterfaceSubClass, iface.bInterfaceProtocol) &&
            iface.bInterfaceClass == 0x03) {
            KLOG("scan: %s VID=0x%04x PID=0x%04x -> generic HID\n", path, vid, pid);
            *out_vid = vid; *out_pid = pid;
            close(fd);
            return &g_generic_hid_desc;
        }
        close(fd);
        return NULL;
    }

    if (!ctrl_is_external_ugen(path)) { close(fd); return NULL; }

    struct usb_fs_endpoint ep; struct usb_fs_init ini; struct usb_fs_uninit u;
    memset(&ep,0,sizeof(ep)); memset(&ini,0,sizeof(ini));
    ini.pEndpoints=&ep; ini.ep_index_max=1;
    if (ioctl(fd,USB_FS_INIT,&ini)!=0) {close(fd);return NULL;}
    { int ii=0; ioctl(fd,USB_IFACE_DRIVER_DETACH,&ii);
      ii=1; ioctl(fd,USB_IFACE_DRIVER_DETACH,&ii);
      ii=2; ioctl(fd,USB_IFACE_DRIVER_DETACH,&ii); }
    struct usb_fs_open po; memset(&po,0,sizeof(po));
    po.ep_index=0; po.max_bufsize=64; po.max_frames=1;
    po.ep_no=0x81;
    if(ioctl(fd,USB_FS_OPEN,&po)==0 && po.max_packet_length==64){
        struct usb_fs_close pc; memset(&pc,0,sizeof(pc)); pc.ep_index=0;
        ioctl(fd,USB_FS_CLOSE,&pc);
        *out_vid=VID_SWITCH; *out_pid=PID_SWITCH; goto done_probe;
    }
    memset(&po,0,sizeof(po)); po.ep_index=0; po.max_bufsize=64; po.max_frames=1;
    po.ep_no=0x82;
    if(ioctl(fd,USB_FS_OPEN,&po)==0 && po.max_packet_length>0 && po.max_packet_length<=64){
        struct usb_fs_close pc; memset(&pc,0,sizeof(pc)); pc.ep_index=0;
        ioctl(fd,USB_FS_CLOSE,&pc);
        *out_vid=VID_XBOX; *out_pid=PID_XBOX; goto done_probe;
    }
done_probe:
    memset(&u,0,sizeof(u)); ioctl(fd,USB_FS_UNINIT,&u); close(fd);
    return ctrl_lookup(*out_vid,*out_pid);
}

/* ── Read / inject loop ───────────────────────────────────────────────── */

static int read_one_poll(int fd, struct usb_fs_endpoint eps[4],
                         uint8_t *buf, uint32_t rptsz, uint32_t nf, uint32_t to_ms)
{
    void *pbufs[8]; uint32_t plens[8];
    uint32_t n_frames = (nf > 0 && nf <= 8) ? nf : 1;
    for (uint32_t fi = 0; fi < n_frames; fi++) {
        pbufs[fi] = buf + fi * rptsz; plens[fi] = rptsz;
    }
    eps[0].ppBuffer = pbufs; eps[0].pLength = plens; eps[0].nFrames = n_frames;
    eps[0].timeout = to_ms; eps[0].aFrames = 0; eps[0].status = 0;
    eps[0].flags = USB_FS_FLAG_SINGLE_SHORT_OK | USB_FS_FLAG_MULTI_SHORT_OK;

    struct usb_fs_start st; memset(&st, 0, sizeof(st)); st.ep_index = 0;
    int sr = ioctl(fd, USB_FS_START, &st);
    if (sr != 0) {
        if (errno == EBUSY) return 1;
        if (errno == ENXIO || errno == ENOTTY) return -errno;
        return 0;
    }
    struct pollfd pfd; pfd.fd = fd;
    pfd.events = POLLIN|POLLOUT|POLLRDNORM|POLLWRNORM; pfd.revents = 0;
    if (poll(&pfd, 1, (int)to_ms) <= 0) {
        struct usb_fs_stop sp; memset(&sp,0,sizeof(sp)); sp.ep_index=0;
        ioctl(fd, USB_FS_STOP, &sp);
        return 0;
    }
    struct usb_fs_complete co; memset(&co,0,sizeof(co)); co.ep_index=0;
    if (ioctl(fd, USB_FS_COMPLETE, &co) != 0) {
        int e = errno;
        if (e == ENXIO || e == ENOTTY) return -e;
        return 0;
    }
    if (eps[0].aFrames == 0 || plens[0] == 0) return 0;
    return (int)plens[0];
}

static int read_one_busypoll(int fd, struct usb_fs_endpoint eps[4],
                             uint8_t *buf, uint32_t sz, uint32_t to_ms)
{
    void *pbufs[1]={buf}; uint32_t plens[1]={sz};
    eps[0].ppBuffer=pbufs; eps[0].pLength=plens; eps[0].nFrames=1;
    eps[0].timeout=to_ms; eps[0].aFrames=0; eps[0].status=0;
    eps[0].flags=USB_FS_FLAG_SINGLE_SHORT_OK|USB_FS_FLAG_MULTI_SHORT_OK;

    struct usb_fs_start st; memset(&st,0,sizeof(st)); st.ep_index=0;
    if (ioctl(fd,USB_FS_START,&st)!=0) {
        if (errno==ENXIO||errno==ENOTTY) return -errno;
        return 0;
    }
    int polls=(int)((to_ms+50)/50)+2;
    for (int w=0; w<polls; w++) {
        struct usb_fs_complete co; memset(&co,0,sizeof(co)); co.ep_index=0;
        if (ioctl(fd,USB_FS_COMPLETE,&co)==0) {
            if (eps[0].aFrames==0||plens[0]==0) return 0;
            return (int)plens[0];
        }
        if (errno==ENXIO||errno==ENOTTY) return -errno;
        if (errno!=EBUSY) {
            struct usb_fs_stop sp; memset(&sp,0,sizeof(sp)); sp.ep_index=0;
            ioctl(fd,USB_FS_STOP,&sp); return -errno;
        }
        usleep(50000);
    }
    struct usb_fs_stop sp; memset(&sp,0,sizeof(sp)); sp.ep_index=0;
    ioctl(fd,USB_FS_STOP,&sp); return 0;
}

int ctrl_run(const char *path, const CtrlDesc *desc,
             int32_t vdi_handle, int slot,
             void (*on_frame)(int slot, const ScePadData *pad))
{
    struct usb_fs_endpoint eps[4]; int fd=-1;
    int hs_state=HS_STREAMING; uint8_t seq=0;

    if(!desc||!desc->ops||!desc->ops->parse){KLOG("slot[%d] ctrl_run: missing ops\n",slot);return -1;}
    memset(eps,0,sizeof(eps));

    if(desc->ops->init){
        if(desc->ops->init(path,&fd,eps,&hs_state,&seq)<0||fd<0){
            KLOG("slot[%d] %s init failed\n",slot,desc->name);
            if(fd>=0){close(fd);fd=-1;} return -1;
        }
    }else{
        fd=open(path,O_RDWR);
        if(fd<0){KLOG("slot[%d] %s open errno=%d\n",slot,desc->name,errno);return -1;}
    }
    KLOG("slot[%d] %s streaming\n",slot,desc->name);

    uint8_t  buf[CTRL_REPORT_MAX];
    uint32_t rptsz=desc->report_size?desc->report_size:64u;
    uint32_t nf=desc->n_frames?desc->n_frames:1u;
    uint32_t fl=desc->flags; uint32_t inj=0;
    int use_poll=(fl&CTRL_FLAG_POLL_READ)!=0;
    int do_stop=(fl&CTRL_FLAG_PER_READ_STOP)!=0;
    int cache_ok=(fl&CTRL_FLAG_CACHED_STATE)!=0;
    if(nf>8)nf=1; if(rptsz>CTRL_REPORT_MAX)rptsz=CTRL_REPORT_MAX;

    ScePadData cp; memset(&cp,0,sizeof(cp)); cp.quat.w=1.0f;
    cp.leftStick.x=128;cp.leftStick.y=128;cp.rightStick.x=128;cp.rightStick.y=128;
    int cv=0, ndc=0;

    while(1){
        memset(buf,0,sizeof(buf));
        int n=use_poll?read_one_poll(fd,eps,buf,rptsz,nf,200)
                      :read_one_busypoll(fd,eps,buf,rptsz,200);
        if(n<0){KLOG("slot[%d] read err=%d gone\n",slot,-n);break;}

        if(use_poll&&n==1){
            struct pollfd pfd; pfd.fd=fd;
            pfd.events=POLLIN|POLLOUT|POLLRDNORM|POLLWRNORM; pfd.revents=0;
            if(poll(&pfd,1,50)<=0){
                if(cache_ok){
                    if(cv&&++ndc>=2){cp.buttons=0;cp.analogButtons.l2=0;cp.analogButtons.r2=0;}
                    if(cv){extern int32_t scePadVirtualDeviceInsertData(int32_t,const void*);scePadVirtualDeviceInsertData(vdi_handle,&cp);}
                }
                continue;
            }
            struct usb_fs_complete co; memset(&co,0,sizeof(co)); co.ep_index=0;
            if(ioctl(fd,USB_FS_COMPLETE,&co)!=0)continue;
            void **pb=(void**)eps[0].ppBuffer;
            n=(eps[0].aFrames>0&&eps[0].pLength[0]>0)?(int)eps[0].pLength[0]:0;
            if(n>0&&pb[0]) memcpy(buf,pb[0],(size_t)n>sizeof(buf)?sizeof(buf):(size_t)n);
        }
        if(n==0){
            if(cache_ok){
                if(cv&&++ndc>=2){cp.buttons=0;cp.analogButtons.l2=0;cp.analogButtons.r2=0;}
                if(cv){extern int32_t scePadVirtualDeviceInsertData(int32_t,const void*);scePadVirtualDeviceInsertData(vdi_handle,&cp);}
            }
            continue;
        }

        ScePadData pd; memset(&pd,0,sizeof(pd)); pd.connected=1; pd.quat.w=1.0f;
        int pr=desc->ops->parse(fd,eps,buf,(uint32_t)n,&pd,&hs_state,&seq);
        if(pr<0){KLOG("slot[%d] parse err\n",slot);break;}
        if(pr==0) goto after;

        {extern int32_t scePadVirtualDeviceInsertData(int32_t,const void*);scePadVirtualDeviceInsertData(vdi_handle,&pd);}
        inj++;
        if(cache_ok){memcpy(&cp,&pd,sizeof(cp));cv=1;ndc=0;}
        if(on_frame) on_frame(slot,&pd);
        if((inj%600)==0) KLOG("slot[%d] VDI #%u\n",slot,inj);

after:
        if(do_stop){struct usb_fs_stop sp;memset(&sp,0,sizeof(sp));sp.ep_index=0;ioctl(fd,USB_FS_STOP,&sp);}
    }
    KLOG("slot[%d] %s exit (%u frames)\n",slot,desc->name,inj);

    if(desc->ops->deinit){desc->ops->deinit(fd,eps);}
    else{
        struct usb_fs_stop sp; memset(&sp,0,sizeof(sp)); sp.ep_index=0; ioctl(fd,USB_FS_STOP,&sp);
        struct usb_fs_close fc; memset(&fc,0,sizeof(fc)); fc.ep_index=0; ioctl(fd,USB_FS_CLOSE,&fc);
        struct usb_fs_uninit u; memset(&u,0,sizeof(u)); ioctl(fd,USB_FS_UNINIT,&u);
        if(fd>=0)close(fd);
    }
    return 0;
}
