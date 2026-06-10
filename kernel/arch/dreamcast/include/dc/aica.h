/* KallistiOS ##version##

   dc/aica.h
   Copyright (C) 2026 Paul Cercueil

   ARM support routines for using the wavetable channels
*/

#ifndef __AICA_H
#define __AICA_H

#include <stdbool.h>
#include <stdint.h>

typedef enum aica_sample_type {
    AICA_SAMPLE_16BIT,
    AICA_SAMPLE_8BIT,
    AICA_SAMPLE_ADPCM,
    AICA_SAMPLE_ADPCM_LS,
} aica_smtype_t;

void aica_init(void);
void aica_shutdown(void);

void aica_set_sample(uint8_t ch, aica_smtype_t type,
                     uint32_t base_addr, uint16_t loop_start,
                     uint16_t loop_end, bool loop);
void aica_set_vol(uint8_t ch, uint8_t vol);
void aica_set_pan(uint8_t ch, uint8_t pan);
void aica_set_freq(uint8_t ch, uint32_t freq);

void aica_start(uint8_t chn);
void aica_stop(uint8_t chn);
bool aica_is_started(uint8_t chn);

int8_t aica_reserve_channel(void);
void aica_unreserve_channel(uint8_t ch);
uint64_t aica_reserved_channels(void);

uint16_t aica_get_pos_unlocked(uint8_t chn);

#endif  /* __AICA_H */
