/* KallistiOS ##version##

   arch/dreamcast/include/arch/thread.h
   Copyright (C) 2026 Paul Cercueil <paul@crapouillou.net>

*/

/** \file   arch/thread.h
    \brief  Thread handling

    This file contains the arch API used to implement threads.

    \author Paul Cercueil
*/

#ifndef __ARCH_THREAD_H
#define __ARCH_THREAD_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <errno.h>
#include <stdint.h>

static inline int arch_thd_block_now(void) {
    __asm__ inline("trapa #0xff" ::: "memory");

    int ret __asm__("r0");
    return ret;
}

__END_DECLS

#endif /* __ARCH_THREAD_H */
