/* KallistiOS ##version##

   aica.c
   (c)2000-2002 Megan Potter
   (c)2024 Stefanos Kornilios Mitsis Poiitidis

   ARM support routines for using the wavetable channels
*/

#include <aicaos/aica.h>
#include <aicaos/irq.h>
#include <cmd_iface.h>
#include <registers.h>

void aica_init(void) {
    int i, j;

    /* Initialize AICA channels */
    SPU_REG32(REG_SPU_MASTER_VOL) = 0;

    for(i = 0; i < 64; i++) {
        SPU_REG32(REG_SPU_PLAY_CTRL(i)) = SPU_FIELD_PREP(SPU_PLAY_CTRL_KEY, 0x2);

        for(j = 4; j < 0x80; j += 4)
            SPU_REG32(CHN_REG(i, j)) = 0;

        SPU_REG32(REG_SPU_AMP_ENV2(i)) = SPU_FIELD_PREP(SPU_AMP_ENV2_RELEASE, 0x1f);
    }

    SPU_REG32(REG_SPU_MASTER_VOL) = SPU_FIELD_PREP(SPU_MASTER_VOL_VOL, 0xf);
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

/* Sets up a sound channel completely. This is generally good if you want
   a quick and dirty way to play notes. If you want a more comprehensive
   set of routines (more like PC wavetable cards) see below.

   ch is the channel to play on (0 - 63)
   smpptr is the pointer to the sound data; if you're running off the
     SH4, then this ought to be (ptr - 0xa0800000); otherwise it's just
     ptr. Basically, it's an offset into sound ram.
   mode is one of the mode constants (16 bit, 8 bit, ADPCM)
   nsamp is the number of samples to play (not number of bytes!)
   freq is the sampling rate of the sound
   vol is the volume, 0 to 0xff (0xff is louder)
   pan is a panning constant -- 0 is left, 128 is center, 255 is right.

   This routine (and the similar ones) owe a lot to Marcus' sound example --
   I hadn't gotten quite this far into dissecting the individual regs yet. */
void aica_play(uint8_t ch, void *smpptr, uint32_t mode,
               uint32_t loopst, uint32_t loopend, uint32_t freq,
               uint8_t vol, uint8_t pan, uint32_t flags) {
    uint32_t freq_lo, freq_base = 5644800;
    int freq_hi = 7;
    uint32_t playCont;

    /* Stop the channel (if it's already playing) */
    aica_stop(ch);

    /* Need to convert frequency to floating point format
       (freq_hi is exponent, freq_lo is mantissa)
       Formula is freq = 44100*2^freq_hi*(1+freq_lo/1024) */
    while(freq < freq_base && freq_hi > -8) {
        freq_base >>= 1;
        --freq_hi;
    }

    freq_lo = (freq << 10) / freq_base;

    /* Envelope setup. The first of these is the loop point,
       e.g., where the sample starts over when it loops. The second
       is the loop end. This is the full length of the sample when
       you are not looping, or the loop end point when you are (though
       storing more than that is a waste of memory if you're not doing
       volume enveloping). */
    SPU_REG32(REG_SPU_LOOP_START(ch)) = loopst & 0xffff;
    SPU_REG32(REG_SPU_LOOP_END(ch)) = loopend & 0xffff;

    /* Write resulting values */
    SPU_REG32(REG_SPU_PITCH(ch)) =
        SPU_FIELD_PREP(SPU_PITCH_OCT, freq_hi) |
        SPU_FIELD_PREP(SPU_PITCH_FNS, freq_lo);

    /* Convert the incoming pan into a hardware value and set it */
    SPU_REG32(REG_SPU_VOL_PAN(ch)) =
        SPU_FIELD_PREP(SPU_VOL_PAN_VOL, 0xf) |
        SPU_FIELD_PREP(SPU_VOL_PAN_PAN, calc_aica_pan(pan));

    /* turn off Low Pass Filter (LPF);
       convert the incoming volume into a hardware value and set it */
    SPU_REG32(REG_SPU_LPF1(ch)) = SPU_LPF1_OFF |
        SPU_FIELD_PREP(SPU_LPF1_Q, 0x4) |
        SPU_FIELD_PREP(SPU_LPF1_VOL, calc_aica_vol(vol));

    /* If we supported volume envelopes (which we don't yet) then
       this value would set that up. The top 4 bits determine the
       envelope speed. f is the fastest, 1 is the slowest, and 0
       seems to be an invalid value and does weird things). The
       default (below) sets it into normal mode (play and terminate/loop).
    SPU_REG32(REG_SPU_AMP_ENV1(ch)) = 0xf010;
    */
    SPU_REG32(REG_SPU_AMP_ENV1(ch)) =
        SPU_FIELD_PREP(SPU_AMP_ENV1_ATTACK, 0x1f); /* No volume envelope */


    /* Set sample format, buffer address, and looping control. If
       0x0200 mask is set on reg 0, the sample loops infinitely. If
       it's not set, the sample plays once and terminates. We'll
       also set the bits to start playback here. */
    SPU_REG32(REG_SPU_ADDR_L(ch)) = (uint32_t)smpptr & 0xffff;

    playCont = (mode << 7) | ((uint32_t)smpptr >> 16);
    if(flags & AICA_PLAY_LOOP)
        playCont |= SPU_PLAY_CTRL_LOOP;
    if(!(flags & AICA_PLAY_DELAY))
        playCont |= SPU_FIELD_PREP(SPU_PLAY_CTRL_KEY, 0x3); /* key on */

    SPU_REG32(REG_SPU_PLAY_CTRL(ch)) = playCont;
}

/* Start sound on all channels specified by chmap bitmap */
void aica_sync_play(uint64_t chmap) {
    int i = 0;

    while(chmap) {
        if(chmap & 0x1) {
            SPU_REG32(REG_SPU_PLAY_CTRL(i)) |=
                SPU_FIELD_PREP(SPU_PLAY_CTRL_KEY, 0x3);
        }

        i++;
        chmap >>= 1;
    }
}

/* Stop the sound on a given channel */
void aica_stop(uint8_t ch) {
    uint32_t ctrl = SPU_REG32(REG_SPU_PLAY_CTRL(ch));

    ctrl = (ctrl & ~SPU_PLAY_CTRL_KEY) |
        SPU_FIELD_PREP(SPU_PLAY_CTRL_KEY, 0x2);

    SPU_REG32(REG_SPU_PLAY_CTRL(ch)) = ctrl;
}


/* The rest of these routines can change the channel in mid-stride so you
   can do things like vibrato and panning effects. */

/* Set channel volume */
void aica_vol(uint8_t ch, uint8_t vol) {
    uint32_t lpf1 = SPU_REG32(REG_SPU_LPF1(ch));

    lpf1 = (lpf1 & ~SPU_LPF1_VOL) |
        SPU_FIELD_PREP(SPU_LPF1_VOL, calc_aica_vol(vol));

    SPU_REG32(REG_SPU_LPF1(ch)) = lpf1;
}

/* Set channel pan */
void aica_pan(uint8_t ch, uint8_t pan) {
    SPU_REG32(REG_SPU_VOL_PAN(ch)) =
        SPU_FIELD_PREP(SPU_VOL_PAN_VOL, 0xf) |
        SPU_FIELD_PREP(SPU_VOL_PAN_PAN, calc_aica_pan(pan));
}

/* Set channel frequency */
void aica_freq(uint8_t ch, uint32_t freq) {
    uint32_t freq_lo, freq_base = 5644800;
    int freq_hi = 7;

    while(freq < freq_base && freq_hi > -8) {
        freq_base >>= 1;
        freq_hi--;
    }

    freq_lo = (freq << 10) / freq_base;
    SPU_REG32(REG_SPU_PITCH(ch)) =
        SPU_FIELD_PREP(SPU_PITCH_OCT, freq_hi) |
        SPU_FIELD_PREP(SPU_PITCH_FNS, freq_lo);
}

/* Get channel position */
uint16_t aica_get_pos(uint8_t ch) {
    uint32_t val;
    int i;

    irq_disable_scoped();

    /* Observe channel ch */
    val = SPU_REG32(REG_SPU_INFO_REQUEST);
    SPU_REG32(REG_SPU_INFO_REQUEST) =
        (val & ~SPU_INFO_REQUEST_REQ) |
        SPU_FIELD_PREP(SPU_INFO_REQUEST_REQ, ch);

    /* Wait a while */
    for(i = 0; i < 20; i++)
        __asm__ volatile ("nop");  /* Prevent loop from being optimized out */

    /* Update position counters */
    return SPU_REG32(REG_SPU_INFO_PLAY_POS) & 0xffff;
}
