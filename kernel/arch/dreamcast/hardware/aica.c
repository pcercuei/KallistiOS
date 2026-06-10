/* KallistiOS ##version##

   aica.c
   Copyright (C) 2000-2002 Megan Potter
   Copyright (C) 2024 Stefanos Kornilios Mitsis Poiitidis
   Copyright (C) 2026 Paul Cercueil

   AICA channels API
*/

#include <kos/irq.h>
#include <kos/regfield.h>
#include <dc/aica.h>
#include <dc/aica_registers.h>

#include <stdatomic.h>
#include <stdbool.h>

enum aica_play_ctrl_key {
    AICA_PLAY_KEY_OFF = 0x2,
    AICA_PLAY_KEY_ON = 0x3,
};

/* Channels mask in inversed order (bit 0 is channel 63, bit 63 is channel 0) */
static uint64_t channels_mask;

void aica_init(void) {
    int i, j;

    /* Initialize AICA channels */
    SPU_REG32(REG_SPU_MASTER_VOL) = 0;

    for(i = 0; i < 64; i++) {
        SPU_REG32(REG_SPU_PLAY_CTRL(i)) =
            FIELD_PREP(SPU_PLAY_CTRL_KEY, AICA_PLAY_KEY_OFF);

        for(j = 4; j < 0x80; j += 4)
            SPU_REG32(CHN_REG(i, j)) = 0;

        SPU_REG32(REG_SPU_AMP_ENV2(i)) = FIELD_PREP(SPU_AMP_ENV2_RELEASE, 0x1f);
    }

    /* Set master volume to max */
    SPU_REG32(REG_SPU_MASTER_VOL) = FIELD_PREP(SPU_MASTER_VOL_VOL, 0xf);

    /* Init CDDA volume and panning */
    SPU_REG32(REG_SPU_CDDA_LEFT) =
        FIELD_PREP(SPU_CDDA_VOL, 0xf) | FIELD_PREP(SPU_CDDA_PAN, 0xf);
    SPU_REG32(REG_SPU_CDDA_RIGHT) =
        FIELD_PREP(SPU_CDDA_VOL, 0xf) | FIELD_PREP(SPU_CDDA_PAN, 0x1f);
}

void aica_shutdown(void) {
    /* Reset the channels */
    aica_init();
}

/* Translates a volume from linear form to logarithmic form (required by
   the AICA chip

    Calculated by
        for (int i = 0; i < 256; i++)
            if (i == 0)
                logs[i] = 255;
            else
                logs[i] = 16.0 * log2(255.0 / i);
   */
static uint8_t logs[] = {
    255, 127, 111, 102, 95, 90, 86, 82, 79, 77, 74, 72, 70, 68, 66, 65,
    63, 62, 61, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 50, 49, 48,
    47, 47, 46, 45, 45, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39,
    38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 34, 33, 33, 33, 32, 32,
    31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 28, 27, 27, 27, 27,
    26, 26, 26, 25, 25, 25, 25, 24, 24, 24, 24, 23, 23, 23, 23, 22,
    22, 22, 22, 21, 21, 21, 21, 20, 20, 20, 20, 20, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16,
    15, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13,
    13, 13, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 10,
    10, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8,
    8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6,
    6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static inline uint8_t calc_aica_vol(uint8_t x) {
    return logs[x];
}

static inline uint8_t calc_aica_pan(uint8_t x) {
    if(x == 0x80)
        return 0;
    else if(x < 0x80) {
        return 0x10 | ((0x7f - x) >> 3);
    }
    else {
        return (x - 0x80) >> 3;
    }
}

void aica_set_sample(uint8_t ch, aica_smtype_t type,
                     uint32_t base_addr, uint16_t loop_start,
                     uint16_t loop_end, bool loop) {
    uint32_t ctrl = SPU_REG32(REG_SPU_PLAY_CTRL(ch)) & SPU_PLAY_CTRL_KEY;

    ctrl |= (type << 7) | (base_addr >> 16);
    if(loop)
        ctrl |= SPU_PLAY_CTRL_LOOP;

    /* Set sample format, buffer address, and looping control. If
       0x0200 mask is set on reg 0, the sample loops infinitely. If
       it's not set, the sample plays once and terminates. We'll
       also set the bits to start playback here. */
    SPU_REG32(REG_SPU_ADDR_L(ch)) = base_addr & 0xffff;
    SPU_REG32(REG_SPU_PLAY_CTRL(ch)) = ctrl;

    /* Envelope setup. The first of these is the loop point,
       e.g., where the sample starts over when it loops. The second
       is the loop end. This is the full length of the sample when
       you are not looping, or the loop end point when you are (though
       storing more than that is a waste of memory if you're not doing
       volume enveloping). */
    SPU_REG32(REG_SPU_LOOP_START(ch)) = loop_start;
    SPU_REG32(REG_SPU_LOOP_END(ch)) = loop_end;

    /* If we supported volume envelopes (which we don't yet) then
       this value would set that up. The top 4 bits determine the
       envelope speed. f is the fastest, 1 is the slowest, and 0
       seems to be an invalid value and does weird things). The
       default (below) sets it into normal mode (play and terminate/loop).
    SPU_REG32(REG_SPU_AMP_ENV1(ch)) = 0xf010;
    */
    SPU_REG32(REG_SPU_AMP_ENV1(ch)) =
        FIELD_PREP(SPU_AMP_ENV1_ATTACK, 0x1f); /* No volume envelope */
}

void aica_set_vol(uint8_t ch, uint8_t vol) {
    /* turn off Low Pass Filter (LPF);
       convert the incoming volume into a hardware value and set it */
    SPU_REG32(REG_SPU_LPF1(ch)) = SPU_LPF1_OFF |
        FIELD_PREP(SPU_LPF1_Q, 0x4) |
        FIELD_PREP(SPU_LPF1_VOL, calc_aica_vol(vol));
}

void aica_set_pan(uint8_t ch, uint8_t pan) {
    /* Convert the incoming pan into a hardware value and set it */
    SPU_REG32(REG_SPU_VOL_PAN(ch)) =
        FIELD_PREP(SPU_VOL_PAN_VOL, 0xf) |
        FIELD_PREP(SPU_VOL_PAN_PAN, calc_aica_pan(pan));
}

/* Set channel frequency */
void aica_set_freq(uint8_t ch, uint32_t freq) {
    uint32_t freq_lo, freq_base = 5644800;
    int freq_hi = 7;

    /* Need to convert frequency to floating point format
       (freq_hi is exponent, freq_lo is mantissa)
       Formula is freq = 44100*2^freq_hi*(1+freq_lo/1024) */
    while(freq < freq_base && freq_hi > -8) {
        freq_base >>= 1;
        freq_hi--;
    }

    freq_lo = (freq << 10) / freq_base;

    /* Write resulting values */
    SPU_REG32(REG_SPU_PITCH(ch)) =
        FIELD_PREP(SPU_PITCH_OCT, freq_hi) |
        FIELD_PREP(SPU_PITCH_FNS, freq_lo);
}

void aica_start(uint8_t chn) {
    SPU_REG32(REG_SPU_PLAY_CTRL(chn)) |=
        FIELD_PREP(SPU_PLAY_CTRL_KEY, AICA_PLAY_KEY_ON);
}

void aica_stop(uint8_t chn) {
    uint32_t ctrl = SPU_REG32(REG_SPU_PLAY_CTRL(chn));

    ctrl = (ctrl & ~SPU_PLAY_CTRL_KEY) |
        FIELD_PREP(SPU_PLAY_CTRL_KEY, AICA_PLAY_KEY_OFF);

    SPU_REG32(REG_SPU_PLAY_CTRL(chn)) = ctrl;
}

bool aica_is_started(uint8_t chn) {
    uint32_t ctrl = SPU_REG32(REG_SPU_PLAY_CTRL(chn));

    return FIELD_GET(ctrl, SPU_PLAY_CTRL_KEY) == AICA_PLAY_KEY_ON;
}

/* Get channel position */
uint16_t aica_get_pos_unlocked(uint8_t ch) {
    uint32_t val;
    int i;

    /* Observe channel ch */
    val = SPU_REG32(REG_SPU_INFO_REQUEST);
    SPU_REG32(REG_SPU_INFO_REQUEST) =
        (val & ~SPU_INFO_REQUEST_REQ) |
        FIELD_PREP(SPU_INFO_REQUEST_REQ, ch);

    /* Wait a while */
    for(i = 0; i < 20; i++)
        __asm__ volatile ("nop");  /* Prevent loop from being optimized out */

    /* Update position counters */
    return SPU_REG32(REG_SPU_INFO_PLAY_POS);
}

int8_t aica_reserve_channel(void)
{
    uint64_t channels = channels_mask;
    uint8_t ch;

    do {
        if (channels == (uint64_t)-1)
            return -1;

        ch = __builtin_ffsll(~channels) - 1;
    } while(!atomic_compare_exchange_weak(&channels_mask, &channels,
                                          channels | (1ull << ch)));

    return 63 - ch;
}

void aica_unreserve_channel(uint8_t ch)
{
    uint64_t channels = channels_mask;

    while(!atomic_compare_exchange_weak(&channels_mask, &channels,
                                          channels & ~(1ull << ch)))
    {
    }
}

uint64_t aica_reserved_channels(void)
{
    return atomic_load(&channels_mask);
}
