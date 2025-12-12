/* KallistiOS ##version##

   timer.c
   Copyright (C) 2026 Paul Cercueil

   Timer API
*/

#include <kos/irq.h>
#include <kos/regfield.h>
#include <aica/aica.h>
#include <aica/queue.h>
#include <arch/timer.h>
#include <stdatomic.h>

static uint8_t counter_channel;

static timer_primary_callback_t tp_callback;

timer_primary_callback_t timer_primary_set_callback(timer_primary_callback_t callback) {
    timer_primary_callback_t old = tp_callback;

    tp_callback = callback;

    return old;
}

void timer_primary_wakeup(uint32_t millis) {
    uint32_t div = 0, ticks = millis * 44100 / 1000;

    if(ticks > 255 << SPU_TIMER_CTRL_DIV_128)
        ticks = 255 << SPU_TIMER_CTRL_DIV_128;

    while(ticks > 255) {
        ticks >>= 1;
        div++;
    }

    /* Re-program the timer to the next event */
    SPU_REG32(REG_SPU_TIMER0_CTRL) =
        FIELD_PREP(SPU_TIMER_CTRL_START, 256 - ticks) |
        FIELD_PREP(SPU_TIMER_CTRL_DIV, div);

    /* Re-enable timer */
    SPU_REG32(REG_SPU_INT_RESET) = SPU_INT_ENABLE_TIMER0;
}

static void timer_irq(irq_t code, irq_context_t *context, void *data) {
    (void)code;
    (void)context;
    (void)data;

    if(tp_callback)
        tp_callback(context);
}

int timer_init(void) {
    aica_chn_data_t counter_data;

    /* Use a regular channel as a counter.
     * Since timers are not readable, we use this channel to read how much time
     * has elapsed since the last time a timer was programmed. */
    counter_channel = aica_reserve_channel();

    counter_data = (aica_chn_data_t){
        .freq = 32768,
        .loopend = 0xffff,
        .type = AICA_SAMPLE_ADPCM,
        .flags = AICA_CHN_DATA_LOOP,
    };

    aica_update(counter_channel, &counter_data);
    aica_start(counter_channel);

    irq_set_handler(EXC_TIMER, timer_irq, NULL);

    return 0;
}

void timer_shutdown(void) {
    irq_set_handler(EXC_TIMER, NULL, NULL);
}

static uint64_t timer_count;
static uint16_t last_timer_value;

uint64_t __aica_get_ticks(void) {
    uint16_t value;

    irq_disable_scoped();

    /* Read timer, if it is < last read, add 0x10000 to the count.
     * Note that this works only if called frequently enough so that the timer
     * won't flip twice. Given that it is called by the scheduler it should work
     * fine. */
    value = aica_get_pos_unlocked(counter_channel);

    if (value < last_timer_value)
        timer_count += 0x10000;

    last_timer_value = value;

    return timer_count + value;
}
