// Comprehensive PS4 compat stubs for OpenOrbis libc++
// Provides missing POSIX/FreeBSD symbols the PS4 stub C library doesn't have.

#pragma once

// Prevent <stdatomic.h> from being included or its macros leaking into C++ mode.
// C++ already has <atomic> and the C11 kill_dependency macro conflicts with it.
#ifdef __cplusplus
#ifndef _STDATOMIC_H
#define _STDATOMIC_H
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>

// ── clockid_t ─────────────────────────────────────────────────────
#ifndef _CLOCKID_T_DECLARED
#define _CLOCKID_T_DECLARED
typedef int clockid_t;
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

// ── locale_t ──────────────────────────────────────────────────────
#ifndef _LOCALE_T_DECLARED
#define _LOCALE_T_DECLARED
typedef void *locale_t;
#endif

#define LC_GLOBAL_LOCALE ((locale_t)-1)

// ── nanosleep / clock_nanosleep ───────────────────────────────────
struct timespec;

static inline int nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)req; (void)rem;
    return 0;
}

static inline int clock_nanosleep(clockid_t clock_id, int flags,
                                   const struct timespec *req, struct timespec *rem) {
    (void)clock_id; (void)flags; (void)req; (void)rem;
    return 0;
}

// ── Locale management ─────────────────────────────────────────────
static inline locale_t uselocale(locale_t newloc) { (void)newloc; return (locale_t)0; }
static inline locale_t newlocale(int mask, const char *loc, locale_t base) {
    (void)mask; (void)loc; (void)base;
    return (locale_t)0;
}
static inline void freelocale(locale_t loc) { (void)loc; }
static inline locale_t duplocale(locale_t loc) { (void)loc; return (locale_t)0; }

// Locale category masks
#ifndef LC_COLLATE_MASK
#define LC_COLLATE_MASK  (1 << 0)
#define LC_CTYPE_MASK    (1 << 1)
#define LC_MESSAGES_MASK (1 << 2)
#define LC_MONETARY_MASK (1 << 3)
#define LC_NUMERIC_MASK  (1 << 4)
#define LC_TIME_MASK     (1 << 5)
#define LC_ALL_MASK      (LC_COLLATE_MASK|LC_CTYPE_MASK|LC_MESSAGES_MASK|LC_MONETARY_MASK|LC_NUMERIC_MASK|LC_TIME_MASK)
#endif

// ── Ctype _l functions ────────────────────────────────────────────
#include <ctype.h>

static inline int isascii(int c) { return (c >= 0 && c <= 127); }

static inline int isxdigit_l(int c, locale_t loc) { (void)loc; return isxdigit(c); }
static inline int isdigit_l(int c, locale_t loc)  { (void)loc; return isdigit(c); }
static inline int isspace_l(int c, locale_t loc)  { (void)loc; return isspace(c); }
static inline int islower_l(int c, locale_t loc)  { (void)loc; return islower(c); }
static inline int isupper_l(int c, locale_t loc)  { (void)loc; return isupper(c); }
static inline int isalpha_l(int c, locale_t loc)  { (void)loc; return isalpha(c); }
static inline int isalnum_l(int c, locale_t loc)  { (void)loc; return isalnum(c); }
static inline int isprint_l(int c, locale_t loc)  { (void)loc; return isprint(c); }
static inline int iscntrl_l(int c, locale_t loc)  { (void)loc; return iscntrl(c); }
static inline int ispunct_l(int c, locale_t loc)  { (void)loc; return ispunct(c); }
static inline int isblank_l(int c, locale_t loc)  { (void)loc; return isblank(c); }
static inline int isgraph_l(int c, locale_t loc)  { (void)loc; return isgraph(c); }
static inline int tolower_l(int c, locale_t loc)  { (void)loc; return tolower(c); }
static inline int toupper_l(int c, locale_t loc)  { (void)loc; return toupper(c); }

// ── Wide char stubs ───────────────────────────────────────────────
#include <wchar.h>

static inline size_t wcsnrtombs(char *dest, const wchar_t **src, size_t nwc,
                                 size_t len, mbstate_t *ps) {
    (void)dest; (void)src; (void)nwc; (void)len; (void)ps;
    return (size_t)-1;
}
static inline size_t mbsnrtowcs(wchar_t *dest, const char **src, size_t nmc,
                                 size_t len, mbstate_t *ps) {
    (void)dest; (void)src; (void)nmc; (void)len; (void)ps;
    return (size_t)-1;
}

// ── vasprintf ─────────────────────────────────────────────────────
static inline int vasprintf(char **strp, const char *fmt, va_list ap) {
    (void)fmt; (void)ap;
    *strp = (char*)"";
    return 0;
}

// ── ftello / fseeko ───────────────────────────────────────────────
static inline off_t ftello(FILE *stream) {
    (void)stream;
    return 0;
}
static inline int fseeko(FILE *stream, off_t offset, int whence) {
    (void)stream; (void)offset; (void)whence;
    return 0;
}

// ── popen / pclose ────────────────────────────────────────────────
static inline FILE *popen(const char *command, const char *type) {
    (void)command; (void)type;
    return (FILE*)0;
}
static inline int pclose(FILE *stream) {
    (void)stream;
    return -1;
}

// ── fdopen ────────────────────────────────────────────────────────
static inline FILE *fdopen(int fd, const char *mode) {
    (void)fd; (void)mode;
    return (FILE*)0;
}

// ── IFF_LOOPBACK ──────────────────────────────────────────────────
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif

// ── Misc ──────────────────────────────────────────────────────────
#ifndef L_tmpnam
#define L_tmpnam 1024
#endif


// Ensure kill_dependency from C headers doesn't leak into C++ <atomic>
#ifdef kill_dependency
#undef kill_dependency
#endif

#ifdef __cplusplus
}
#endif

// ── usleep ───────────────────────────────────────────────────────
#include <time.h>
static inline int usleep(unsigned int usec) {
    struct timespec ts;
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;
    return nanosleep(&ts, NULL);
}
