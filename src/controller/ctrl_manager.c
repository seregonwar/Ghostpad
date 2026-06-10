/* SPDX-License-Identifier: GPL-3.0-or-later
 * ctrl_manager.c — USB controller manager implementation
 *
 * Slot management, hotplug scan loop, VDA creation + force_bind, klog feed.
 * All previously in gc_main.c — now a module callable from Ghostpad's main.
 */

#include "ctrl_manager.h"
#include "controller.h"
#include "ctrl_registry.h"
#include <unistd.h>
#include "../shellui_pad.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef __PROSPERO__
#include <ps5/kernel.h>
#include <ps5/klog.h>
#include <ps5/mdbg.h>
#endif

/* ── Logging (uses Ghostpad's ghostpad_status_log) ──────────────────── */
#define gp_log(...) ghostpad_status_log("[GC] " __VA_ARGS__)

/* ── SCE stubs ────────────────────────────────────────────────────────── */
extern int32_t scePadGetHandle(int32_t userId, int32_t type, int32_t index);
extern int32_t scePadVirtualDeviceAddDevice(void *param, int32_t deviceType);
extern int32_t scePadVirtualDeviceDeleteDevice(int32_t handle);
extern int32_t sceKernelSendNotificationRequest(int unk0, void *req, size_t size, int unk1);

#define VIRTUAL_DEVICE_TYPE_DUALSENSE 3

/* ── Slots ────────────────────────────────────────────────────────────── */

#define MAX_SLOTS 4

typedef struct {
    volatile int32_t handle;
    volatile int     vdi_ready;
    volatile int     usb_active;
    volatile int     confirmed;
    char             dev_path[32];
    uint16_t         vid, pid;
    volatile uint32_t inject_count;
    volatile int     stop;     /* set to ask thread to exit */
} ctrl_slot_t;

static ctrl_slot_t     g_slots[MAX_SLOTS];
static pthread_mutex_t g_slot_lock = PTHREAD_MUTEX_INITIALIZER;
static int32_t         g_inject_uid = 0x10000000;
static volatile int    g_assign_slot = -1;

/* ── klog device-ID queue (fed by Ghostpad's parse_klog_line) ────────── */

#define KLOG_QSIZE 16
static uint64_t        g_klog_q[KLOG_QSIZE];
static int             g_klog_qw = 0, g_klog_qr = 0;
static pthread_mutex_t g_klog_lock = PTHREAD_MUTEX_INITIALIZER;

void ctrl_manager_on_device_id(uint64_t id) {
    if (!id) return;
    pthread_mutex_lock(&g_klog_lock);
    int next = (g_klog_qw + 1) % KLOG_QSIZE;
    if (next != g_klog_qr) { g_klog_q[g_klog_qw] = id; g_klog_qw = next; }
    pthread_mutex_unlock(&g_klog_lock);
}

static uint64_t klog_dequeue_ms(int ms) {
    for (int t = 0; t < ms; t += 100) {
        pthread_mutex_lock(&g_klog_lock);
        if (g_klog_qw != g_klog_qr) {
            uint64_t id = g_klog_q[g_klog_qr];
            g_klog_qr = (g_klog_qr + 1) % KLOG_QSIZE;
            pthread_mutex_unlock(&g_klog_lock);
            return id;
        }
        pthread_mutex_unlock(&g_klog_lock);
        usleep(100000);
    }
    return 0;
}

/* ── Notification ─────────────────────────────────────────────────────── */

typedef struct { char _unk[45]; char message[3075]; } NotifyRequest;

static void notify(const char *fmt, ...) {
    NotifyRequest req; va_list ap;
    memset(&req, 0, sizeof(req));
    va_start(ap, fmt); vsnprintf(req.message, sizeof(req.message), fmt, ap); va_end(ap);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

/* ── VDA creation for a slot ──────────────────────────────────────────── */

static int32_t create_vda_for_slot(int slot) {
    struct { int32_t size; int32_t userId; int32_t pad[6]; } vdp;
    const int32_t SEN = (int32_t)0xDEADBEEFu;
    memset(&vdp, 0, sizeof(vdp)); vdp.size = sizeof(vdp); vdp.userId = 1;
    for (int k = 0; k < 6; k++) vdp.pad[k] = SEN;

    int ret = scePadVirtualDeviceAddDevice(&vdp, VIRTUAL_DEVICE_TYPE_DUALSENSE);
    gp_log("slot[%d] VDA ret=0x%08x\n", slot, (uint32_t)ret);

    int32_t handle = (ret > 0) ? ret : -1;
    for (int k = 0; k < 6; k++) {
        if (vdp.pad[k] != SEN && vdp.pad[k] > 0) { if (handle < 0) handle = vdp.pad[k]; break; }
    }

    uint64_t dev_id = klog_dequeue_ms(10000);
    if (dev_id) {
        handle = (int32_t)(dev_id & 0xffffffffu);
        int br = shellui_pad_force_bind(dev_id, g_inject_uid);
        gp_log("slot[%d] force_bind(0x%llx, 0x%08x) ret=%d\n",
               slot, (unsigned long long)dev_id, (uint32_t)g_inject_uid, br);
    } else if (handle >= 0) {
        gp_log("slot[%d] klog timeout — using direct handle %d\n", slot, handle);
    } else {
        gp_log("slot[%d] GetHandle scan...\n", slot);
        static const int32_t uids[] = { 1, 0x10000000, (int32_t)0xffffffff };
        for (int ui = 0; ui < 3 && handle < 0; ui++)
            for (int idx = 0; idx < 8 && handle < 0; idx++) {
                handle = scePadGetHandle(uids[ui], 3, idx);
                if (handle >= 0) gp_log("slot[%d] GetHandle uid=0x%08x idx=%d h=%d\n",
                                        slot, (uint32_t)uids[ui], idx, handle);
            }
    }
    if (handle >= 0)
        gp_log("slot[%d] VDI handle=0x%x ready\n", slot, (uint32_t)handle);
    else
        gp_log("slot[%d] ERROR: no VDA handle\n", slot);
    return handle;
}

/* ── Per-frame callback (called by ctrl_run after each injection) ─────── */

static void on_controller_frame(int slot, const ScePadData *pad) {
    if (!g_slots[slot].confirmed && pad->buttons != 0) {
        g_slots[slot].confirmed = 1;
        if (g_assign_slot == slot) g_assign_slot = -1;
        gp_log("slot[%d] assignment confirmed (button press)\n", slot);
    }
}

/* ── USB controller thread ────────────────────────────────────────────── */

typedef struct {
    int              slot;
    char             dev_path[32];
    const CtrlDesc  *desc;
} usb_thread_arg_t;

static void *usb_ctrl_thread(void *arg) {
    usb_thread_arg_t *targ = (usb_thread_arg_t *)arg;
    int slot = targ->slot;
    char dev_path[32]; memcpy(dev_path, targ->dev_path, sizeof(dev_path));
    const CtrlDesc *desc = targ->desc;
    free(targ);

    const char *json_name2 = ctrl_registry_get_name(g_slots[slot].vid, g_slots[slot].pid);
    const char *disp_name2 = json_name2 ? json_name2 : desc->name;
    gp_log("slot[%d] ctrl thread: %s (%s)\n", slot, dev_path, disp_name2);
    notify("Ghostpad: slot[%d] %s streaming", slot, disp_name2);

    ctrl_run(dev_path, desc, g_slots[slot].handle, slot, on_controller_frame);

    /* Device disconnected */
    gp_log("slot[%d] ctrl thread exiting\n", slot);
    notify("Ghostpad: slot[%d] disconnected", slot);
    scePadVirtualDeviceDeleteDevice(g_slots[slot].handle);

    pthread_mutex_lock(&g_slot_lock);
    g_slots[slot].handle     = -1;
    g_slots[slot].vdi_ready  = 0;
    g_slots[slot].usb_active = 0;
    g_slots[slot].dev_path[0]= '\0';
    pthread_mutex_unlock(&g_slot_lock);

    return NULL;
}

/* ── Controller manager thread (hotplug scan loop) ────────────────────── */

static void *controller_manager_thread(void *arg) {
    (void)arg;
    int scan = 0;
    gp_log("manager: started (MAX_SLOTS=%d)\n", MAX_SLOTS);

    while (1) {
        char ugen_paths[32][32];
        int n_paths = 0;
        DIR *dp = opendir("/dev");
        if (dp) {
            struct dirent *ent;
            while (n_paths < 32 && (ent = readdir(dp)) != NULL) {
                if (strncmp(ent->d_name, "ugen", 4) != 0) continue;
                const char *dot = strchr(ent->d_name, '.');
                if (!dot || dot[1] == '\0') continue;
                if (strcmp(dot, ".1") == 0) continue;
                snprintf(ugen_paths[n_paths], sizeof(ugen_paths[0]), "/dev/%s", ent->d_name);
                n_paths++;
            }
            closedir(dp);
        }
        if ((scan % 5) == 0)
            gp_log("manager: scan #%d — %d ugen paths\n", scan, n_paths);

        for (int i = 0; i < n_paths; i++) {
            if (g_assign_slot >= 0) break;

            const char *path = ugen_paths[i];
            int busy = 0;
            pthread_mutex_lock(&g_slot_lock);
            for (int s = 0; s < MAX_SLOTS; s++) {
                if (g_slots[s].usb_active && strcmp(g_slots[s].dev_path, path) == 0)
                    { busy = 1; break; }
            }
            pthread_mutex_unlock(&g_slot_lock);
            if (busy) continue;

            uint16_t vid = 0, pid = 0;
            const CtrlDesc *desc = ctrl_probe(path, &vid, &pid);
            if (!desc) continue;

            int slot = -1;
            pthread_mutex_lock(&g_slot_lock);
            for (int s = 0; s < MAX_SLOTS; s++) {
                if (g_slots[s].handle < 0 && !g_slots[s].usb_active)
                    { slot = s; break; }
            }
            pthread_mutex_unlock(&g_slot_lock);

            if (slot < 0) { if ((scan%5)==0) gp_log("manager: all slots full\n"); continue; }

            const char *json_name = ctrl_registry_get_name(vid, pid);
            const char *disp_name = json_name ? json_name : desc->name;
            gp_log("manager: %s at %s -> slot[%d]\n", disp_name, path, slot);
            notify("Ghostpad: %s connected", disp_name);

            pthread_mutex_lock(&g_slot_lock);
            strncpy(g_slots[slot].dev_path, path, sizeof(g_slots[slot].dev_path)-1);
            g_slots[slot].usb_active = 1;
            g_slots[slot].confirmed  = 0;
            g_slots[slot].vid = vid; g_slots[slot].pid = pid;
            pthread_mutex_unlock(&g_slot_lock);
            g_assign_slot = slot;

            int32_t handle = create_vda_for_slot(slot);
            if (handle < 0) {
                gp_log("manager: slot[%d] VDA failed\n", slot);
                pthread_mutex_lock(&g_slot_lock);
                g_slots[slot].usb_active  = 0;
                g_slots[slot].dev_path[0] = '\0';
                g_slots[slot].handle      = -1;
                pthread_mutex_unlock(&g_slot_lock);
                g_assign_slot = -1;
                continue;
            }

            pthread_mutex_lock(&g_slot_lock);
            g_slots[slot].handle       = handle;
            g_slots[slot].vdi_ready    = 1;
            g_slots[slot].inject_count = 0;
            pthread_mutex_unlock(&g_slot_lock);

            usb_thread_arg_t *targ = malloc(sizeof(*targ));
            if (!targ) {
                gp_log("manager: malloc fail slot[%d]\n", slot);
                scePadVirtualDeviceDeleteDevice(handle);
                pthread_mutex_lock(&g_slot_lock);
                g_slots[slot].handle=-1; g_slots[slot].vdi_ready=0;
                g_slots[slot].usb_active=0; g_slots[slot].dev_path[0]='\0';
                pthread_mutex_unlock(&g_slot_lock);
                g_assign_slot = -1;
                continue;
            }
            targ->slot = slot;
            strncpy(targ->dev_path, path, sizeof(targ->dev_path)-1);
            targ->desc = desc;

            pthread_t tid;
            if (pthread_create(&tid, NULL, usb_ctrl_thread, targ) != 0) {
                gp_log("manager: pthread_create fail slot[%d]\n", slot);
                free(targ);
                scePadVirtualDeviceDeleteDevice(handle);
                pthread_mutex_lock(&g_slot_lock);
                g_slots[slot].handle=-1; g_slots[slot].vdi_ready=0;
                g_slots[slot].usb_active=0; g_slots[slot].dev_path[0]='\0';
                pthread_mutex_unlock(&g_slot_lock);
                g_assign_slot = -1;
            } else {
                pthread_detach(tid);
                gp_log("manager: slot[%d] ctrl thread started handle=0x%x\n",
                       slot, (uint32_t)handle);
                notify("Ghostcontrol: slot[%d] ready — press a button to assign", slot);
            }
            break;
        }

        /* Assignment timeout: 6 * 2s = 12s */
        static int assign_wait = 0;
        if (g_assign_slot >= 0) {
            if (++assign_wait > 6) {
                gp_log("manager: assignment timeout slot[%d]\n", g_assign_slot);
                g_assign_slot = -1;
                assign_wait = 0;
            }
        } else { assign_wait = 0; }

        scan++;
        if ((scan % 5) == 0) {
            int active = 0;
            for (int s = 0; s < MAX_SLOTS; s++) if (g_slots[s].usb_active) active++;
            if (active == 0 && (scan%10)==0) gp_log("manager: scan #%d — no controllers\n", scan);
        }
        usleep(2000000);
    }
    return NULL;
}

/* ── Public API ───────────────────────────────────────────────────────── */

int ctrl_manager_init(int32_t userId, int32_t injectUid) {
    (void)userId;
    g_inject_uid = injectUid;

    /* Load external VID/PID database.  If missing, disable USB detection. */
    if (ctrl_registry_init() != 0) {
        notify("Ghostpad: controllers.json missing in /data/ghostpad/");
        notify("Ghostpad: USB detection disabled — only TCP mode available");
        gp_log("manager: controllers.json not found — USB detection disabled\n");
        return -2;
    }

    for (int s = 0; s < MAX_SLOTS; s++) {
        g_slots[s].handle     = -1;
        g_slots[s].vdi_ready  = 0;
        g_slots[s].usb_active = 0;
        g_slots[s].confirmed  = 0;
        g_slots[s].dev_path[0]= '\0';
        g_slots[s].stop       = 0;
    }
    g_assign_slot = -1;

    /* Clean up orphaned VDA devices */
    for (int dh = 0; dh < 64; dh++) {
        if (scePadVirtualDeviceDeleteDevice(dh) == 0)
            gp_log("deleteDevice(%d)\n", dh);
    }
    return 0;
}

int ctrl_manager_start(void) {
    pthread_t mgr_tid;
    if (pthread_create(&mgr_tid, NULL, controller_manager_thread, NULL) == 0) {
        pthread_detach(mgr_tid);
        gp_log("manager: thread started\n");
        return 0;
    }
    gp_log("manager: pthread_create failed\n");
    return -1;
}

void ctrl_manager_cleanup(void) {
    for (int s = 0; s < MAX_SLOTS; s++) {
        g_slots[s].stop = 1;
        if (g_slots[s].handle >= 0)
            scePadVirtualDeviceDeleteDevice(g_slots[s].handle);
    }
}
