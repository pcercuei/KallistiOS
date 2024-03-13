/* KallistiOS ##version##

   aica.c
   (c)2000-2002 Megan Potter

   ARM support routines for using the wavetable channels
*/

#include "aica_cmd_iface.h"
#include "aica_registers.h"
#include "aica.h"
#include "irq.h"

/* Channels mask in inversed order (bit 0 is channel 63, bit 63 is channel 0) */
static unsigned long long channels_mask;

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

void counter_init(unsigned char ch) {
    aica_play(ch, (void *)0, AICA_SM_ADPCM, 0, 0xffff,
	      44100, 0, 0, AICA_PLAY_LOOP);
}

/* Translates a volume from linear form to logarithmic form (required by
   the AICA chip */
static unsigned char logs[] = {
    0, 15, 22, 27, 31, 35, 39, 42, 45, 47, 50, 52, 55, 57, 59, 61,
    63, 65, 67, 69, 71, 73, 74, 76, 78, 79, 81, 82, 84, 85, 87, 88,
    90, 91, 92, 94, 95, 96, 98, 99, 100, 102, 103, 104, 105, 106,
    108, 109, 110, 111, 112, 113, 114, 116, 117, 118, 119, 120, 121,
    122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
    135, 136, 137, 138, 138, 139, 140, 141, 142, 143, 144, 145, 146,
    146, 147, 148, 149, 150, 151, 152, 152, 153, 154, 155, 156, 156,
    157, 158, 159, 160, 160, 161, 162, 163, 164, 164, 165, 166, 167,
    167, 168, 169, 170, 170, 171, 172, 173, 173, 174, 175, 176, 176,
    177, 178, 178, 179, 180, 181, 181, 182, 183, 183, 184, 185, 185,
    186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 193, 193, 194,
    195, 195, 196, 197, 197, 198, 199, 199, 200, 200, 201, 202, 202,
    203, 204, 204, 205, 205, 206, 207, 207, 208, 209, 209, 210, 210,
    211, 212, 212, 213, 213, 214, 215, 215, 216, 216, 217, 217, 218,
    219, 219, 220, 220, 221, 221, 222, 223, 223, 224, 224, 225, 225,
    226, 227, 227, 228, 228, 229, 229, 230, 230, 231, 232, 232, 233,
    233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 239, 239, 240,
    240, 241, 241, 242, 242, 243, 243, 244, 244, 245, 245, 246, 246,
    247, 247, 248, 248, 249, 249, 250, 250, 251, 251, 252, 252, 253, 254, 255
};

static inline unsigned char calc_aica_vol(unsigned char x) {
    return 0xff - logs[x];
}

static inline unsigned char calc_aica_pan(unsigned char x) {
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
void aica_play(unsigned char ch, void *smpptr, unsigned int mode,
	       unsigned int loopst, unsigned int loopend, unsigned int freq,
	       unsigned char vol, unsigned char pan, unsigned int flags) {
    uint32 freq_lo, freq_base = 5644800;
    int freq_hi = 7;
    uint32 playCont;

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
    SPU_REG32(REG_SPU_ADDR_L(ch)) = (unsigned int)smpptr & 0xffff;

    playCont = (mode << 7) | ((unsigned int)smpptr >> 16);
    if(flags & AICA_PLAY_LOOP)
        playCont |= SPU_PLAY_CTRL_LOOP;
    if(!(flags & AICA_PLAY_DELAY))
        playCont |= SPU_FIELD_PREP(SPU_PLAY_CTRL_KEY, 0x3); /* key on */

    SPU_REG32(REG_SPU_PLAY_CTRL(ch)) = playCont;
}

/* Start sound on all channels specified by chmap bitmap */
void aica_sync_play(unsigned long long chmap) {
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
void aica_stop(unsigned char ch) {
    uint32 ctrl = SPU_REG32(REG_SPU_PLAY_CTRL(ch));

    ctrl = (ctrl & ~SPU_PLAY_CTRL_KEY) |
        SPU_FIELD_PREP(SPU_PLAY_CTRL_KEY, 0x2);

    SPU_REG32(REG_SPU_PLAY_CTRL(ch)) = ctrl;
}


/* The rest of these routines can change the channel in mid-stride so you
   can do things like vibrato and panning effects. */

/* Set channel volume */
void aica_vol(unsigned char ch, unsigned char vol) {
    uint32 lpf1 = SPU_REG32(REG_SPU_LPF1(ch));

    lpf1 = (lpf1 & ~SPU_LPF1_VOL) |
        SPU_FIELD_PREP(SPU_LPF1_VOL, calc_aica_vol(vol));

    SPU_REG32(REG_SPU_LPF1(ch)) = lpf1;
}

/* Set channel pan */
void aica_pan(unsigned char ch, unsigned char pan) {
    SPU_REG32(REG_SPU_VOL_PAN(ch)) =
        SPU_FIELD_PREP(SPU_VOL_PAN_VOL, 0xf) |
        SPU_FIELD_PREP(SPU_VOL_PAN_PAN, calc_aica_pan(pan));
}

/* Set channel frequency */
void aica_freq(unsigned char ch, unsigned int freq) {
    uint32 freq_lo, freq_base = 5644800;
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
unsigned short aica_get_pos(unsigned char ch) {
    unsigned short pos;
    irq_ctx_t cxt;
    uint32 val;
    int i;

    cxt = irq_disable();

    /* Observe channel ch */
    val = SPU_REG32(REG_SPU_INFO_REQUEST);
    SPU_REG32(REG_SPU_INFO_REQUEST) =
        (val & ~SPU_INFO_REQUEST_REQ) |
        SPU_FIELD_PREP(SPU_INFO_REQUEST_REQ, ch);

    /* Wait a while */
    for(i = 0; i < 20; i++)
        __asm__ volatile ("nop");  /* Prevent loop from being optimized out */

    /* Update position counters */
    pos = SPU_REG32(REG_SPU_INFO_PLAY_POS) & 0xffff;

    irq_restore(cxt);

    return pos;
}

unsigned char aica_reserve_channel(void)
{
    unsigned char ch;
    irq_ctx_t cxt;

    cxt = irq_disable();

    ch = __builtin_ffsll(~channels_mask) - 1;
    channels_mask |= 1ull << ch;

    irq_restore(cxt);

    return 63 - ch;
}

void aica_unreserve_channel(unsigned char ch)
{
    irq_ctx_t cxt = irq_disable();

    channels_mask &= ~(1ull << (63 - ch));

    irq_restore(cxt);
}
