/* KallistiOS ##version##

   timer.c
   Copyright (c) 2000, 2001, 2002 Megan Potter
   Copyright (c) 2023 Falco Girgis
*/

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include <arch/arch.h>
#include <arch/timer.h>
#include <arch/irq.h>

/* Register access macros */
#define TIMER8(o)   ( *((volatile uint8_t  *)(TIMER_BASE + (o))) )
#define TIMER16(o)  ( *((volatile uint16_t *)(TIMER_BASE + (o))) )
#define TIMER32(o)  ( *((volatile uint32_t *)(TIMER_BASE + (o))) )

/* Register base address */
#define TIMER_BASE 0xffd80000 

/* Register offsets */
#define TOCR    0x00    /* Timer Output Control Register */
#define TSTR    0x04    /* Timer Start Register */
#define TCOR0   0x08    /* Timer Constant Register 0 */
#define TCNT0   0x0c    /* Timer Counter Register 0 */
#define TCR0    0x10    /* Timer Control Register 0 */
#define TCOR1   0x14    /* Timer Constant Register 1 */
#define TCNT1   0x18    /* Timer Counter Register 1 */
#define TCR1    0x1c    /* Timer Control Register 1 */
#define TCOR2   0x20    /* Timer Constant Register 2 */
#define TCNT2   0x24    /* Timer Counter Register 2 */
#define TCR2    0x28    /* Timer Control Register 2 */
#define TCPR2   0x2c    /* Timer Input Capture */

/* Timer Start Register fields */
#define STR2    2   /* TCNT2 Counter Start */
#define STR1    1   /* TCNT1 Counter Start */
#define STR0    0   /* TCNT0 Counter Start */

/* Timer Control Register fields */
#define UCPF    9   /* Input Capture Interrupt Flag (TMU2 only) */
#define UNF     8   /* Underflow Flag */
#define ICPE1   7   /* Input Capture Control (TMU2 only) */
#define ICP0    6  
#define UNIE    5   /* Underflow Interrupt Control */
#define CKEG1   4   /* Clock Edge */
#define CKEG0   3
#define TPSC2   2   /* Timer Prescalar */
#define TPSC1   1
#define TPSC0   0

/* Timer Prescalar Values (Peripheral clock divided by N) */
typedef enum TPSC { 
    PCK_DIV_4,      /* Pck/4    => 80ns */
    PCK_DIV_16,     /* Pck/16   => 320ns*/
    PCK_DIV_64,     /* Pck/64   => 1280ns*/
    PCK_DIV_256,    /* Pck/256  => 5120ns*/
    PCK_DIV_1024    /* Pck/1024 => 20480ns*/
} TPSC;

/* Timer registers, indexed by Timer ID. */
static unsigned tcors[] = { TCOR0, TCOR1, TCOR2 };
static unsigned tcnts[] = { TCNT0, TCNT1, TCNT2 };
static unsigned tcrs[] = { TCR0, TCR1, TCR2 };
/* Clock divisor value for each TPSC value. */
static unsigned tdiv[] = { 4, 16, 64, 256, 1024 };
/* Nanoseconds per counter tick for each TPSC value. */
static unsigned tns[] = { 80, 320, 1280, 5120, 20480 };

/* Timer TPSC values (4 for highest resolution timings) */
#define TIMER_TPSC      PCK_DIV_64
/* Timer IRQ priority levels (0-15) */
#define TIMER_PRIO      15

/* Apply timer configuration to registers */
static int timer_prime_apply(int which, uint32_t count, int interrupts) { 
    assert(which <= TMU2);

    TIMER32(tcnts[which]) = count;
    TIMER32(tcors[which]) = count; 

    TIMER16(tcrs[which]) = TIMER_TPSC;

    /* Enable IRQ generation plus unmask and set priority */
    if(interrupts) {
        TIMER16(tcrs[which]) |= (1 << UNIE);
        timer_enable_ints(which);
    }

    return 0;
}

/* Pre-initialize a timer; set values but don't start it */
int timer_prime(int which, uint32_t speed, int interrupts) {
    /* Initialize counters; formula is P0/(tps*div) */
    const uint32_t cd = 50000000 / (speed * tdiv[TIMER_TPSC]);

    return timer_prime_apply(which, cd, interrupts);
}

/* Works like timer_prime, but takes an interval in milliseconds
   instead of a rate. Used by the primary timer stuff. */
static int timer_prime_wait(int which, uint32_t millis, int interrupts) {
    /* Calculate the countdown, formula is P0 * millis/div*1000. We
       rearrange the math a bit here to avoid integer overflows. */
    const uint32_t cd = (50000000 / tdiv[TIMER_TPSC]) * millis / 1000;

    return timer_prime_apply(which, cd, interrupts);
}

/* Start a timer -- starts it running (and interrupts if applicable) */
int timer_start(int which) {
    assert(which <= TMU2);

    TIMER8(TSTR) |= 1 << which;
    return 0;
}

/* Stop a timer -- and disables its interrupt */
int timer_stop(int which) {
    assert(which <= TMU2);

    timer_disable_ints(which);

    /* Stop timer */
    TIMER8(TSTR) &= ~(1 << which);

    return 0;
}

/* Returns the count value of a timer */
uint32_t timer_count(int which) {
    assert(which <= TMU2);

    return TIMER32(tcnts[which]);
}

/* Clears the timer underflow bit and returns what its value was */
int timer_clear(int which) {
    assert(which <= TMU2);
    const uint16_t value = TIMER16(tcrs[which]);

    TIMER16(tcrs[which]) &= ~(1 << UNF);
    return !!(value & (1 << UNF));
}

/* Spin-loop kernel sleep func: uses the secondary timer in the
   SH-4 to very accurately delay even when interrupts are disabled */
void timer_spin_sleep(int ms) {
    timer_prime(TMU1, 1000, 0);
    timer_clear(TMU1);
    timer_start(TMU1);

    while(ms > 0) {
        while(!(TIMER16(tcrs[TMU1]) & (1 << UNF)))
            ;

        timer_clear(TMU1);
        ms--;
    }

    timer_stop(TMU1);
}

/* Enable timer interrupts (high priority); needs to move
   to irq.c sometime. */
void timer_enable_ints(int which) {
    volatile uint16_t *ipra = (uint16_t *)0xffd00004;
    *ipra |= (0x000f << (12 - 4 * which));
}

/* Disable timer interrupts; needs to move to irq.c sometime. */
void timer_disable_ints(int which) {
    volatile uint16_t *ipra = (uint16_t *)0xffd00004;
    *ipra &= ~(0x000f << (12 - 4 * which));
}

/* Check whether ints are enabled */
int timer_ints_enabled(int which) {
    volatile uint16_t *ipra = (uint16_t *)0xffd00004;
    return (*ipra & (0x000f << (12 - 4 * which))) != 0;
}

/* Millisecond timer */
static uint32_t timer_ms_counter = 0; /* Seconds elapsed */
static uint32_t timer_ms_countdown;   /* Max counter value */

static void timer_ms_handler(irq_t source, irq_context_t *context) {
    (void)source;
    (void)context;
    timer_ms_counter++;

    /* Clear overflow bit so we can check it when returning time */
    TIMER16(tcrs[TMU2]) &= ~(1 << UNF);
}

void timer_ms_enable(void) {
    irq_set_handler(EXC_TMU2_TUNI2, timer_ms_handler);
    timer_prime(TMU2, 1, 1);
    timer_ms_countdown = timer_count(TMU2);
    timer_clear(TMU2);
    timer_start(TMU2);
}

void timer_ms_disable(void) {
    timer_stop(TMU2);
    timer_disable_ints(TMU2);
}

/* Return the number of ticks since KOS was booted */
static void timer_getticks(uint32_t *secs, uint32_t *ticks, uint32_t div) {
    int irq_status = irq_disable();

    /* Seconds part comes from ms_counter */
    if(secs) {
        *secs = timer_ms_counter;

        /* Overflow is only notable if we have seconds we can
           overflow into, so avoid read of TCR if secs is null */
        if(TIMER16(tcrs[TMU2] & (1 << UNF)))
            (*secs)++;
    }

    if(ticks) {
        assert(timer_ms_countdown > 0);

        const uint64_t ticks64 = (timer_ms_countdown - TIMER32(tcnts[TMU2])) *
                                  tns[tcrs[TMU2] & (TPSC0 | TPSC1 | TPSC2)] /
                                  div;

        assert(ticks64 <= UINT32_MAX);

        *ticks = ticks64;
    }
    irq_restore(irq_status);
}

/* Return the number of ticks since KOS was booted */
void timer_ms_gettime(uint32_t *secs, uint32_t *msecs) {
    timer_getticks(secs, msecs, 1000000);
}

uint64_t timer_ms_gettime64(void) {
    uint32_t s, ms;
    uint64_t msec;

    timer_ms_gettime(&s, &ms);
    msec = ((uint64_t)s) * 1000 + ((uint64_t)ms);

    return msec;
}

void timer_us_gettime(uint32_t *secs, uint32_t *usecs) {
    timer_getticks(secs, usecs, 1000);
}

uint64_t timer_us_gettime64(void) {
    uint32_t s, us;
    uint64_t usec;

    timer_us_gettime(&s, &us);
    usec = ((uint64_t)s) * 1000000 + ((uint64_t)us);

    return usec;
}

void timer_ns_gettime(uint32_t *secs, uint32_t *usecs) {
    timer_getticks(secs, usecs, 1);
}

/* Primary kernel timer. What we'll do here is handle actual timer IRQs
   internally, and call the callback only after the appropriate number of
   millis has passed. For the DC you can't have timers spaced out more
   than about one second, so we emulate longer waits with a counter. */
static timer_primary_callback_t tp_callback;
static uint32_t tp_ms_remaining;

/* IRQ handler for the primary timer interrupt. */
static void tp_handler(irq_t src, irq_context_t *cxt) {
    (void)src;

    /* Are we at zero? */
    if(tp_ms_remaining == 0) {
        /* Disable any further timer events. The callback may
           re-enable them of course. */
        timer_stop(TMU0);
        timer_disable_ints(TMU0);

        /* Call the callback, if any */
        if(tp_callback)
            tp_callback(cxt);
    } 
    /* Do we have less than a second remaining? */
    else if(tp_ms_remaining < 1000) {
        /* Schedule a "last leg" timer. */
        timer_stop(TMU0);
        timer_prime_wait(TMU0, tp_ms_remaining, 1);
        timer_clear(TMU0);
        timer_start(TMU0);
        tp_ms_remaining = 0;
    } 
    /* Otherwise, we're just counting down. */
    else {
        tp_ms_remaining -= 1000;
    }
}

/* Enable / Disable primary kernel timer */
static void timer_primary_init(void) {
    /* Clear out our vars */
    tp_callback = NULL;

    /* Clear out TMU0 and get ready for wakeups */
    irq_set_handler(EXC_TMU0_TUNI0, tp_handler);
    timer_clear(TMU0);
}

static void timer_primary_shutdown(void) {
    timer_stop(TMU0);
    timer_disable_ints(TMU0);
    irq_set_handler(EXC_TMU0_TUNI0, NULL);
}

timer_primary_callback_t timer_primary_set_callback(timer_primary_callback_t cb) {
    timer_primary_callback_t cbold = tp_callback;
    tp_callback = cb;
    return cbold;
}

void timer_primary_wakeup(uint32_t millis) {
    /* Don't allow zero */
    if(millis == 0) {
        assert_msg(millis != 0, "Received invalid wakeup delay");
        millis++;
    }

    /* Make sure we stop any previous wakeup */
    timer_stop(TMU0);

    /* If we have less than a second to wait, then just schedule the
       timeout event directly. Otherwise schedule a periodic second
       timer. We'll replace this on the last leg in the IRQ. */
    if(millis >= 1000) {
        timer_prime_wait(TMU0, 1000, 1);
        timer_clear(TMU0);
        timer_start(TMU0);
        tp_ms_remaining = millis - 1000;
    }
    else {
        timer_prime_wait(TMU0, millis, 1);
        timer_clear(TMU0);
        timer_start(TMU0);
        tp_ms_remaining = 0;
    }
}


/* Init */
int timer_init(void) {
    /* Disable all timers */
    TIMER8(TSTR) = 0;

    /* Set to internal clock source */
    TIMER8(TOCR) = 0;

    /* Setup the primary timer stuff */
    timer_primary_init();

    return 0;
}

/* Shutdown */
void timer_shutdown(void) {
    /* Shutdown primary timer stuff */
    timer_primary_shutdown();

    /* Disable all timers */
    TIMER8(TSTR) = 0;
    timer_disable_ints(TMU0);
    timer_disable_ints(TMU1);
    timer_disable_ints(TMU2);
}

/* Quick access macros */
#define PMCR_CTRL(o)  ( *((volatile uint16*)(0xff000084) + (o << 1)) )
#define PMCTR_HIGH(o) ( *((volatile uint32*)(0xff100004) + (o << 1)) )
#define PMCTR_LOW(o)  ( *((volatile uint32*)(0xff100008) + (o << 1)) )

#define PMCR_CLR        0x2000
#define PMCR_PMST       0x4000
#define PMCR_PMENABLE   0x8000
#define PMCR_RUN        0xc000
#define PMCR_PMM_MASK   0x003f

#define PMCR_CLOCK_TYPE_SHIFT 8

/* 5ns per count in 1 cycle = 1 count mode(PMCR_COUNT_CPU_CYCLES) */
#define NS_PER_CYCLE      5

/* Get a counter's current configuration */
uint16 perf_cntr_get_config(int which) {
    return PMCR_CTRL(which);
}

/* Start a performance counter */
int perf_cntr_start(int which, int mode, int count_type) {
    perf_cntr_clear(which);
    PMCR_CTRL(which) = PMCR_RUN | mode | (count_type << PMCR_CLOCK_TYPE_SHIFT);

    return 0;
}

/* Stop a performance counter */
int perf_cntr_stop(int which) {
    PMCR_CTRL(which) &= ~(PMCR_PMM_MASK | PMCR_PMENABLE);

    return 0;
}

/* Clears a performance counter.  Has to stop it first. */
int perf_cntr_clear(int which) {
    perf_cntr_stop(which);
    PMCR_CTRL(which) |= PMCR_CLR;

    return 0;
}

/* Returns the count value of a counter */
inline uint64 perf_cntr_count(int which) {
    return (uint64)(PMCTR_HIGH(which) & 0xffff) << 32 | PMCTR_LOW(which);
}

void timer_ns_enable(void) {
    perf_cntr_start(PRFC0, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
}

void timer_ns_disable(void) {
    uint16 config = PMCR_CTRL(PRFC0);

    /* If timer is running, disable it */
    if((config & PMCR_ELAPSED_TIME_MODE)) {
        perf_cntr_clear(PRFC0);
    }
}

inline uint64 timer_ns_gettime64(void) {
    uint16 config = PMCR_CTRL(PRFC0);

    /* If timer is running */
    if((config & PMCR_ELAPSED_TIME_MODE)) {
        uint64 cycles = perf_cntr_count(PRFC0);
        return cycles * NS_PER_CYCLE;
    }
    else {
        uint64 micro_secs = timer_us_gettime64();
        return micro_secs * 1000;
    }
}
