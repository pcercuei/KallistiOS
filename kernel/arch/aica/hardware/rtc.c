/* KallistiOS ##version##

   rtc.c
   Copyright (C) 2026 Paul Cercueil
*/

#include <arch/rtc.h>
#include <kos/timer.h>

#include <stdint.h>
#include <errno.h>

/*
    High 16-bit Timestamp Value

    32-bit register containing the upper 16-bits of
    the 32-bit timestamp in seconds. Only the lower 16-bits
    are valid.

    Writing to this register will lock the timestamp registers.
*/
#define RTC_TIMESTAMP_HIGH_ADDR   0x00810000

/*
    Low 16-bit Timestamp Value

    32-bit register containing the lower 16-bits of
    the 32-bit timestamp in seconds. Only the lower 16-bits
    are valid.
*/
#define RTC_TIMESTAMP_LOW_ADDR    0x00810004

/*
    Timestamp Control Register

    All fields are reserved except for RTC_CTRL_WRITE_EN,
    which is write-only.
*/
#define RTC_CTRL_ADDR             0x00810008

/*
    Timestamp Write Enable

    RTC_CTRL_ADDR field to be written in order to unlock
    writing to the timestamp registers.
*/
#define RTC_CTRL_WRITE_EN         (1 << 0)

/*
   Second Delta between Sega and Unix Epochs

   Twenty years in seconds.
*/
#define RTC_UNIX_EPOCH_DELTA    631152000

/*
    # of Read/Write Retry Attempts

    To ensure a coherent, race-free read/write operation.
*/
#define RTC_RETRY_COUNT         3

/* The boot time; we'll save this in rtc_init() */
time_t aica_boot_time;

/* Returns the date/time value as a UNIX epoch time stamp */
time_t arch_rtc_unix_secs(void) {
    uint32_t rtcold, rtcnew;
    int i;

    /* Try several times to make sure we don't read one value, then the
       clock increments itself, then we read the second value. This
       algorithm is from NetBSD. */
    rtcold = 0;

    for(;;) {
        for(i = 0; i < RTC_RETRY_COUNT; i++) {
            rtcnew = ((*(volatile uint32_t *)RTC_TIMESTAMP_HIGH_ADDR & 0xffff) << 16) |
                      (*(volatile uint32_t *)RTC_TIMESTAMP_LOW_ADDR & 0xffff);

            if(rtcnew != rtcold)
                break;
        }

        if(i < RTC_RETRY_COUNT)
            rtcold = rtcnew;
        else
            break;
    }

    return rtcnew - RTC_UNIX_EPOCH_DELTA;
}

int arch_rtc_set_unix_secs(time_t secs) {
    (void)secs;

    /* Sorry, we don't support setting the RTC from the ARM side */
    errno = EPERM;
    return -1;
}
