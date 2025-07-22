/* KallistiOS ##version##

   include/kos/timer.h
   Copyright (C) 2025 Paul Cercueil
*/

/** \file    kos/timer.h
    \brief   Timer functionality.
    \ingroup timers

    This file contains functions for reading the internal timer provided
    by the architecture.

    \sa arch/timer.h

    \author Paul Cercueil
*/
#ifndef __KOS_TIMER_H
#define __KOS_TIMER_H

#include <sys/cdefs.h>
__BEGIN_DECLS

#include <kos/tuple.h>

/** \brief   Get the current uptime of the system (in seconds and milliseconds).
    \ingroup timers

    This function retrieves the number of seconds and milliseconds since KOS was
    started.

    \return                 A tuple type, where the first value is the number of
                            seconds, and the second value is the number of
                            milliseconds.
*/
tu32_t timer_get_time_ms(void);

/** \brief   Get the current uptime of the system (in seconds and microseconds).
    \ingroup timers

    This function retrieves the number of seconds and microseconds since KOS was
    started.

    \return                 A tuple type, where the first value is the number of
                            seconds, and the second value is the number of
                            microseconds.
*/
tu32_t timer_get_time_us(void);

/** \brief   Get the current uptime of the system (in seconds and nanoseconds).
    \ingroup timers

    This function retrieves the number of seconds and nanoseconds since KOS was
    started.

    \return                 A tuple type, where the first value is the number of
                            seconds, and the second value is the number of
                            nanoseconds.
*/
tu32_t timer_get_time_ns(void);

__END_DECLS

#endif /* __KOS_TIMER_H */
