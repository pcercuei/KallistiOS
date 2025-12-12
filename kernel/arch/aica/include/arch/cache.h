/* KallistiOS ##version##

   arch/aica/include/arch/cache.h
   Copyright (C) 2026 Paul Cercueil
*/

/* Keep this include above the macro guards */
#include <kos/cache.h>

#ifndef __ARCH_CACHE_H
#define __ARCH_CACHE_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <kos/regfield.h>

#include <stdalign.h>
#include <stdint.h>

#define ARCH_CACHE_L1_ICACHE_SIZE       0
#define ARCH_CACHE_L1_ICACHE_ASSOC      1
#define ARCH_CACHE_L1_ICACHE_LINESIZE   32

#define ARCH_CACHE_L1_DCACHE_SIZE       0
#define ARCH_CACHE_L1_DCACHE_ASSOC      1
#define ARCH_CACHE_L1_DCACHE_LINESIZE   32

#define ARCH_CACHE_L2_CACHE_SIZE        0
#define ARCH_CACHE_L2_CACHE_ASSOC       0
#define ARCH_CACHE_L2_CACHE_LINESIZE    0

static inline void arch_icache_inval_range(uintptr_t start, size_t count) {
    (void)start;
    (void)count;
}

static inline void arch_icache_sync_range(uintptr_t start, size_t count) {
    (void)start;
    (void)count;
}

static inline void arch_dcache_pref_line(const void *src) {
    (void)src;
}

static inline void arch_dcache_alloc_line_with_value(void *src, uintptr_t value) {
    *(uintptr_t *)((uintptr_t)src & ~0x1f) = value;
}

static inline void arch_dcache_alloc_line(void *src) {
    (void)src;
}

static inline void arch_dcache_zero_alloc_line(void *src) {
    uint32_t *ptr = (uint32_t *)((uintptr_t)src & ~0x1f);

    ptr[0] = ptr[1] = ptr[2] = ptr[3] = ptr[4] = ptr[5] = ptr[6] = ptr[7] = 0;
}

static inline void arch_dcache_inval_line(void *src) {
    (void)src;
}

static inline void arch_dcache_purge_line(void *src) {
    (void)src;
}

static inline void arch_dcache_wback_line(void *src) {
    (void)src;
}

static inline void arch_dcache_inval_range(uintptr_t start, size_t count) {
    (void)start;
    (void)count;
}

static inline void arch_dcache_wback_all(void) {
}

static inline void arch_dcache_wback_range(uintptr_t start, size_t count) {
    (void)start;
    (void)count;
}

static inline void arch_dcache_purge_all(void) {
}

static inline void arch_dcache_purge_range(uintptr_t start, size_t count) {
    (void)start;
    (void)count;
}

/** @} */

__END_DECLS

#endif  /* __ARCH_CACHE_H */
