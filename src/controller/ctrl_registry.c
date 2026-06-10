/* SPDX-License-Identifier: GPL-3.0-or-later
 * ctrl_registry.c — JSON VID/PID database loader (backed by sJson)
 *
 * Loads /data/ghostpad/controllers.json.
 * Uses sJson for full RFC 8259 validation with structured error reporting.
 * No built-in fallback — if the file is missing or malformed, returns -1.
 *
 * Self-repair: entries with missing/invalid vid/pid fields are skipped.
 * The rest of the file continues to load.
 */

#define JSON_IMPLEMENTATION
#define JSON_MAX_DEPTH        8       /* our JSON is shallow */
#define JSON_MAX_NODES        2048    /* ~42 entries × 6 nodes ≈ 252; generous */
#define JSON_MAX_STRING_LEN   256     /* controller names are short */
#define JSON_MAX_ARRAY_LEN    256     /* max entries per category */
#include "json_pal.h"

#include "ctrl_registry.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __PROSPERO__
#include <ps5/klog.h>
#define KLOG(...) klog_printf("[GC] " __VA_ARGS__)
#else
#define KLOG(...) fprintf(stderr, __VA_ARGS__)
#endif

#define JSON_PATH "/data/ghostpad/controllers.json"
#define MAX_ENTRIES   256

typedef struct { uint16_t vid; uint16_t pid; char name[48]; } entry_t;
static entry_t g_ds4[MAX_ENTRIES];
static int     g_ds4_count = 0;
static int     g_loaded    = 0;

/* ── Parse one {"vid":"0x...","pid":"0x...","name":"..."} entry via sJson ─ */

static int parse_entry(const JsonValue *obj, entry_t *e)
{
    JsonValue *v;
    const char *s;
    uint32_t len;

    memset(e, 0, sizeof(*e));

    /* vid (required) */
    if (json_obj_get(obj, "vid", &v) != JSON_OK) return 0;
    if (json_get_string(v, &s, &len) != JSON_OK) return 0;
    if (len < 3) return 0;
    {
        const char *p = s;
        uint16_t val = 0;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        for (uint32_t i = 0; i < len && p < s + len; i++, p++) {
            char c = *p;
            val <<= 4;
            if      (c >= '0' && c <= '9') val |= (uint16_t)(c - '0');
            else if (c >= 'a' && c <= 'f') val |= (uint16_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (uint16_t)(c - 'A' + 10);
            else return 0;
        }
        if (val == 0) return 0;
        e->vid = val;
    }

    /* pid (required) */
    if (json_obj_get(obj, "pid", &v) != JSON_OK) return 0;
    if (json_get_string(v, &s, &len) != JSON_OK) return 0;
    if (len < 3) return 0;
    {
        const char *p = s;
        uint16_t val = 0;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        for (uint32_t i = 0; i < len && p < s + len; i++, p++) {
            char c = *p;
            val <<= 4;
            if      (c >= '0' && c <= '9') val |= (uint16_t)(c - '0');
            else if (c >= 'a' && c <= 'f') val |= (uint16_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (uint16_t)(c - 'A' + 10);
            else return 0;
        }
        if (val == 0) return 0;
        e->pid = val;
    }

    /* name (optional, use empty string if missing) */
    if (json_obj_get(obj, "name", &v) == JSON_OK && json_get_string(v, &s, &len) == JSON_OK) {
        size_t n = len < sizeof(e->name) - 1 ? (size_t)len : sizeof(e->name) - 1;
        memcpy(e->name, s, n);
        e->name[n] = '\0';
    }

    return 1;
}

/* ── Parse a category array (e.g. "ds4", "xbox") via sJson ───────────── */

static int parse_category(const JsonValue *arr, entry_t *dst, int max)
{
    uint32_t arr_len, n = 0;

    if (json_get_arr_len(arr, &arr_len) != JSON_OK) return 0;

    for (uint32_t i = 0; i < arr_len && n < max; i++) {
        JsonValue *item;
        if (json_arr_get(arr, i, &item) != JSON_OK) break;
        if (!json_is_object(item)) {
            KLOG("registry: skipping non-object entry at idx %u\n", i);
            continue;  /* self-repair: skip malformed entry */
        }
        if (parse_entry(item, &dst[n]))
            n++;
        else
            KLOG("registry: skipping entry %u (missing vid/pid)\n", i);
    }
    return n;
}

/* ── Load JSON via sJson with validation ──────────────────────────────── */

static JsonArena *g_arena = NULL;

static int load_json(const char *path)
{
    char buf[8192];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = (int)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';

    /* Create sJson arena */
    g_arena = json_arena_create(NULL, 32 * 1024);
    if (!g_arena) { KLOG("registry: arena_create failed\n"); return -1; }

    JsonError err;
    JsonValue *root = json_parse_cstr(g_arena, buf, &err);
    if (!root) {
        KLOG("registry: JSON parse error %d (%s) — file may be corrupted\n",
             err, json_error_str(err));
        json_arena_destroy(g_arena);
        g_arena = NULL;
        return -1;
    }

    if (!json_is_object(root)) {
        KLOG("registry: JSON root is not an object\n");
        json_arena_destroy(g_arena);
        g_arena = NULL;
        return -1;
    }

    /* Navigate: root.ds4[], root.xbox[], etc. */
    JsonValue *ds4_arr;
    if (json_obj_get(root, "ds4", &ds4_arr) == JSON_OK && json_is_array(ds4_arr)) {
        g_ds4_count = parse_category(ds4_arr, g_ds4, MAX_ENTRIES);
        KLOG("registry: parsed %d DS4 entries\n", g_ds4_count);
    }

    /* Future: parse "xbox", "nintendo", "logitech" categories here */
    /* json_obj_get(root, "xbox", ...); etc. */

    return 0;
}

/* ── Public ───────────────────────────────────────────────────────────── */

int ctrl_registry_init(void)
{
    if (g_loaded) return 0;

    if (load_json(JSON_PATH) != 0) {
        KLOG("registry: %s not found or invalid\n", JSON_PATH);
        return -1;
    }
    if (g_ds4_count == 0) {
        KLOG("registry: no valid DS4 entries in JSON\n");
        json_arena_destroy(g_arena);
        g_arena = NULL;
        return -1;
    }
    KLOG("registry: loaded %d DS4 entries from %s\n", g_ds4_count, JSON_PATH);
    g_loaded = 1;
    return 0;
}

int ctrl_registry_is_ds4(uint16_t vid, uint16_t pid)
{
    if (!g_loaded) return 0;
    for (int i = 0; i < g_ds4_count; i++)
        if (g_ds4[i].vid == vid && g_ds4[i].pid == pid) return 1;
    return 0;
}

const char *ctrl_registry_get_name(uint16_t vid, uint16_t pid)
{
    if (!g_loaded) return NULL;
    for (int i = 0; i < g_ds4_count; i++)
        if (g_ds4[i].vid == vid && g_ds4[i].pid == pid) return g_ds4[i].name;
    return NULL;
}
