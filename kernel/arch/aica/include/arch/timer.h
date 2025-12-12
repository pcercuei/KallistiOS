/* KallistiOS ##version##

   arch/aica/include/timer.h
   Copyright (c) 2026 Paul Cercueil

*/

/** \file    arch/timer.h
    \brief   Low-level timer functionality.
    \ingroup timers

    \author Paul Cercueil
*/

#ifndef __ARCH_TIMER_H
#define __ARCH_TIMER_H


#include <stdint.h>
#include <kos/cdefs.h>
__BEGIN_DECLS

#include <kos/irq.h>

#include <time.h>

uint64_t __aica_get_ticks(void);

static inline struct timespec arch_timer_gettime(void) {
    uint64_t ticks = __aica_get_ticks();

    /* Not very accurate math, but our resolution is about 30 µs anyway. */

    return (struct timespec){
        .tv_sec = ticks / 32768,
        .tv_nsec = (ticks % 32768) * (1000000000 / 32768),
    };
}

typedef void (*timer_primary_callback_t)(irq_context_t *);

timer_primary_callback_t timer_primary_set_callback(timer_primary_callback_t callback);

void timer_primary_wakeup(uint32_t millis);

/** \cond */
/* Init function */
int timer_init(void);

/* Shutdown */
void timer_shutdown(void);
/** \endcond */

__END_DECLS

#endif  /* __ARCH_TIMER_H */

