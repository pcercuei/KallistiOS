/* KallistiOS ##version##

   stack.c
   Copyright (C) 2026 Paul Cercueil
*/

#include <kos/dbgio.h>
#include <kos/thread.h>
#include <arch/arch.h>
#include <arch/stack.h>
#include <stdint.h>

/* This function is unnecessary and does nothing on Dreamcast */
void arch_stk_setup(kthread_t *nt) {
    (void)nt;
}

/* Do a stack trace from the current function; leave off the first n frames
   (i.e., in assert()). */
__noinline void arch_stk_trace(int n) {
    arch_stk_trace_at(arch_get_fptr(), n + 1);
}

/* Do a stack trace from the given frame pointer (useful for things like
   tracing from an ISR); leave off the first n frames. */
void arch_stk_trace_at(uint32_t fp, size_t n) {
    (void)fp;
    (void)n;
}

