/* KallistiOS ##version##

   aica/dsp.h
   Copyright (C) 2026 Paul Cercueil

   AICA support routines for the DSP
*/

#ifndef __AICA_DSP_H
#define __AICA_DSP_H

#include <stdint.h>

typedef uint16_t dsp_float16_t;

typedef enum dsp_ringbuffer_size {
    DSP_RB_SIZE_16K,
    DSP_RB_SIZE_32K,
    DSP_RB_SIZE_64K,
    DSP_RB_SIZE_128K,
} dsp_ringbuffer_size_t;

typedef struct dsp_program {
    uint32_t header;
    uint8_t nb_steps;
    uint8_t nb_temps;
    uint8_t nb_coefs;
    uint8_t nb_madrs;
    /* Follows:
       - nb_steps 64-bit words (program),
       - nb_temps 32-bit words (temp buffer),
       - nb_coefs 16-bit words (program coefficients),
       - nb_madrs 16-bit words (address buffer)
     */
} dsp_program_t;

int dsp_set_input_mask(uint64_t mask);

uint64_t dsp_get_input_mask(void);

void dsp_set_input_volume(uint8_t dsp_chn, uint8_t vol);
void dsp_set_output_volume_and_pan(uint8_t dsp_chn, uint8_t vol, uint8_t pan);

dsp_float16_t float_to_dsp(float f);
dsp_float16_t int24_to_dsp(int32_t u);

dsp_float16_t * dsp_alloc_ring_buffer(dsp_ringbuffer_size_t size);

int dsp_load_program(const dsp_program_t *program,
                     dsp_float16_t *rb, dsp_ringbuffer_size_t rb_size);

#endif /* __AICA_DSP_H */
