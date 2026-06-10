/* SPDX-License-Identifier: GPL-3.0-or-later
 * klog.h — /dev/klog reader, MBus event parser, TCP bridge, callbacks
 *
 * Central module for kernel log access.  Used by both Ghostpad's VDA
 * binding logic (main.c) and the USB controller manager (ctrl_manager.c).
 *
 * Usage:
 *   klog_init();                               // start reader + TCP bridge
 *   klog_on_device(my_callback, my_ctx);       // get DEVICE_ADDED device IDs
 *   uint64_t id = klog_dequeue_id(5000);       // blocking convenience
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

/* ── Configuration ────────────────────────────────────────────────────── */

#ifndef KLOG_PORT
#define KLOG_PORT 3434
#endif
#ifndef KLOG_MAX_CLIENTS
#define KLOG_MAX_CLIENTS 4
#endif
#ifndef KLOG_BACKLOG_SIZE
#define KLOG_BACKLOG_SIZE (256 * 1024)
#endif

/* ── Device ID callback type ──────────────────────────────────────────── */

typedef void (*klog_device_cb)(uint64_t dev_id, void *ctx);

/* ── Public API ───────────────────────────────────────────────────────── */

/* Start the /dev/klog reader thread and TCP bridge thread.
 * Returns 0 on success, -1 if /dev/klog can't be opened. */
int klog_init(void);

/* Register a callback for MBus DEVICE_ADDED device IDs.
 * Multiple callbacks can be registered. Thread-safe. */
void klog_on_device(klog_device_cb cb, void *ctx);

/* Blocking dequeue: wait up to timeout_ms for a device ID.
 * Returns 0 on timeout.  Convenience wrapper over callbacks. */
uint64_t klog_dequeue_id(int timeout_ms);

/* ── Utilities ────────────────────────────────────────────────────────── */

/* Case-insensitive substring search. */
const char *klog_strcasestr(const char *haystack, const char *needle);

/* Parse hex string (with optional 0x prefix) to uint64_t. */
uint64_t klog_parse_hex(const char *str);
