/* KallistiOS ##version##

   arch/dreamcast/kernel/tls.c
   Copyright (C) 2024 Falco Girgis
   Copyright (C) 2025 Donald Haase
*/

/* Functions to initialize and manage TLS data. */

#include <assert.h>
#include <kos/thread.h>
#include <arch/tls_static.h>

void arch_tls_init(void) {
    /* Initialize GBR register for Main Thread */
    __builtin_set_thread_pointer((void*)(thd_get_current()->context.gbr));
}

void arch_tls_setup(kthread_t *thd) {
    thd->context.gbr = (uint32_t)thd->tls_hnd;
}
