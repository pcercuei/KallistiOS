/* KallistiOS ##version##

   arch/aica/include/arch/rtc.h
   Copyright (C) 2026 Paul Cercueil

*/

/* Keep this include above the macro guards */
#include <kos/rtc.h>

#ifndef __ARCH_RTC_H
#define __ARCH_RTC_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <time.h>

extern time_t aica_boot_time;

time_t arch_rtc_unix_secs(void);
int arch_rtc_set_unix_secs(time_t time);

static inline time_t arch_rtc_boot_time(void) {
    return aica_boot_time;
}

static inline int arch_rtc_init(void) {
    aica_boot_time = arch_rtc_unix_secs();
    return 0;
}

static inline void arch_rtc_shutdown(void) {
}

__END_DECLS

#endif  /* __ARCH_RTC_H */

