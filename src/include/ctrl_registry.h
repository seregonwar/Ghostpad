/* SPDX-License-Identifier: GPL-3.0-or-later
 * ctrl_registry.h — Runtime-updatable VID/PID database from JSON
 *
 * Loads /data/ghostpad/controllers.json at startup.
 * NO built-in fallback — if the file is missing, USB controller
 * detection is disabled and only TCP injection mode is available.
 */

#pragma once
#include <stdint.h>

/* Load the JSON database.  Returns 0 on success, -1 if file missing. */
int ctrl_registry_init(void);

/* Returns 1 if (vid, pid) is registered as a DS4-protocol device. */
int ctrl_registry_is_ds4(uint16_t vid, uint16_t pid);

/* Returns the human-readable name from the JSON for (vid, pid),
 * or NULL if not found.  Pointer is valid until next init. */
const char *ctrl_registry_get_name(uint16_t vid, uint16_t pid);
