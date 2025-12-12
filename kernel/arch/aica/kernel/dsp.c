/* KallistiOS ##version##

   dsp.c
   Copyright (C) 2026 Paul Cercueil

   DSP API
*/

#include <kos/dbglog.h>
#include <kos/regfield.h>
#include <aica/dsp.h>
#include <aica/registers.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define DSP_REGS_COEF   (AICA_REGISTERS_BASE_ARM + 0x3000)
#define DSP_REGS_MADRS  (AICA_REGISTERS_BASE_ARM + 0x3200)
#define DSP_REGS_MPRO   (AICA_REGISTERS_BASE_ARM + 0x3400)
#define DSP_REGS_TEMP   (AICA_REGISTERS_BASE_ARM + 0x4000)

static uint64_t dsp_mask;

dsp_float16_t * dsp_alloc_ring_buffer(dsp_ringbuffer_size_t size)
{
    return aligned_alloc(2048, 16384 << size);
}

int dsp_load_program(const dsp_program_t *program,
                     dsp_float16_t *rb, dsp_ringbuffer_size_t rb_size)
{
    uint64_t val64;
    uint32_t val32, *dst;
    uintptr_t addr;
    unsigned int i;

    if ((uintptr_t)rb & 0x7ff) {
        /* Ring buffer must be aligned to 2048 bytes */
        return -EINVAL;
    }

    dsp_set_input_mask(0);

    if (rb) {
        /* If we have a ringbuffer, clear it, just to be sure */
        memset(rb, 0, 16384 << rb_size);
    }

    addr = (uintptr_t)program + sizeof(*program);
    dst = (uint32_t *)DSP_REGS_MPRO;

    for (i = 0; i < program->nb_steps; i++) {
        val64 = ((uint64_t *)addr)[i];
        *dst++ = (uint16_t)(val64 >> 48);
        *dst++ = (uint16_t)(val64 >> 32);
        *dst++ = (uint16_t)(val64 >> 16);
        *dst++ = (uint16_t)(val64 >> 0);
    }

    addr += sizeof(uint64_t) * program->nb_steps;
    dst = (uint32_t *)DSP_REGS_TEMP;

    for (i = 0; i < program->nb_temps; i++) {
        val32 = ((uint32_t *)addr)[i];

        *dst++ = (uint8_t)val32;
        *dst++ = (uint16_t)(val32 >> 8);
    }

    addr += sizeof(uint32_t) * program->nb_temps;
    dst = (uint32_t *)DSP_REGS_COEF;

    for (i = 0; i < program->nb_coefs; i++)
        *dst++ = ((uint16_t *)addr)[i] & 0xfff8;

    addr += sizeof(uint32_t) * program->nb_coefs;
    dst = (uint32_t *)DSP_REGS_MADRS;

    for (i = 0; i < program->nb_madrs; i++)
        *dst++ = ((uint16_t *)addr)[i];

    SPU_REG32(REG_SPU_DSP_RB_ADDR) = FIELD_PREP(SPU_DSP_RB_ADDR_SIZE, rb_size) |
        FIELD_PREP(SPU_DSP_RB_ADDR_PTR, (uint32_t)rb >> 11);

    dbglog(DBG_INFO, "DSP program loaded.\n");

    return 0;
}

int dsp_set_input_mask(uint64_t mask) {
    unsigned int i, chn;

    if (__builtin_popcountll(mask) > 16)
        return -EINVAL;

    dsp_mask = mask;

    for (i = 0, chn = 0; i < 64; mask >>= 1, i++) {
        if (mask & 0x1) {
            SPU_REG32(REG_SPU_DSP(i)) = FIELD_PREP(SPU_DSP_SEND, 0xf) |
                FIELD_PREP(SPU_DSP_CHN, chn++);
        } else {
            SPU_REG32(REG_SPU_DSP(i)) = 0;
        }
    }

    return 0;
}

uint64_t dsp_get_input_mask(void) {
    return dsp_mask;
}

void dsp_set_input_volume(uint8_t chn, uint8_t vol) {
    uint32_t ctrl = SPU_REG32(REG_SPU_DSP(chn));

    ctrl = (ctrl & ~SPU_DSP_SEND) | FIELD_PREP(SPU_DSP_SEND, vol >> 4);

    SPU_REG32(REG_SPU_DSP(chn)) = ctrl;
}

static uint8_t dsp_get_dsp_chn(uint8_t chn) {
    unsigned int i, dsp_chn;
    uint64_t mask = dsp_mask;

    for (i = 0, dsp_chn = 0; i < chn; i++, mask >>= 1)
        dsp_chn += mask & 1;

    return dsp_chn;
}

void dsp_set_output_volume_and_pan(uint8_t chn, uint8_t vol, uint8_t pan) {
    uint8_t dsp_chn = dsp_get_dsp_chn(chn);

    SPU_REG32(REG_SPU_DSP_MIXER(dsp_chn)) =
        FIELD_PREP(SPU_DSP_MIXER_VOL, vol >> 4) |
        FIELD_PREP(SPU_DSP_MIXER_PAN, pan >> 3);
}

dsp_float16_t float_to_dsp(float f) {
    union {
        float f;
        uint32_t d;
    } fd;

    fd.f = f;

    return ((fd.d >> 16) & 0xf800) | ((fd.d >> 13) & 0x7ff);
}

dsp_float16_t int24_to_dsp(int32_t u) {
    uint16_t exp;

    u <<= 8;
    exp = __builtin_clrsb(u);
    u <<= exp;
    u >>= 11;

    return ((uint16_t)u & 0x87ff) | (exp << 11);
}
