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

static inline int arch_thd_block_now(void) {
    register volatile int r0 __asm__("r0");

    __asm__ inline("trapa #0");

    return r0;
}

__END_DECLS

#endif /* __ARCH_THREAD_H */
