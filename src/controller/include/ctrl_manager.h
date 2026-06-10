/* SPDX-License-Identifier: GPL-3.0-or-later
 * ctrl_manager.h — USB controller manager (hotplug, slots, VDA, klog feed)
 *
 * Returns:
 *   ctrl_manager_init:   0 = JSON loaded, USB detection active
 *                        -1 = init error
 *                        -2 = controllers.json missing, USB detection disabled
 *   ctrl_manager_start:  0 = manager thread started
 *                        -1 = thread creation failed
 */

#pragma once
#include <stdint.h>

int  ctrl_manager_init(int32_t userId, int32_t injectUid);
int  ctrl_manager_start(void);
void ctrl_manager_on_device_id(uint64_t dev_id);
void ctrl_manager_cleanup(void);
