/* KallistiOS ##version##

   aica/aica.h
   Copyright (C) 2026 Paul Cercueil

   ARM support routines for using the wavetable channels
*/

#ifndef __AICA_AICA_H
#define __AICA_AICA_H

#include <stdint.h>

typedef enum aica_sample_type {
    AICA_SAMPLE_16BIT,
    AICA_SAMPLE_8BIT,
    AICA_SAMPLE_ADPCM,
    AICA_SAMPLE_ADPCM_LS,
} aica_smtype_t;

/* Flags for aica_chn_data_t.flags */
#define AICA_CHN_DATA_LOOP  (1 << 0)

typedef struct aica_channel_data {
    union {
        struct {
            uint32_t    addr;       /**< Sample address in ARM memory space */
            uint32_t    freq;       /**< Playback sample rate */
            uint16_t    loopstart;  /**< Sample loop start */
            uint16_t    loopend;    /**< Sample loop end */
            aica_smtype_t type :8;  /**< Sample type (8/16-bit PCM or 4-bit ADPCM) */
            uint8_t     vol;        /**< Volume */
            uint8_t     pan;        /**< Panning */
            uint8_t     flags;      /**< Misc flags */
        };
        uint32_t raw[4];
    };
} aica_chn_data_t;

void aica_init(aica_chn_data_t *channels);
void aica_shutdown(void);

void aica_update(uint8_t chn, const aica_chn_data_t *data);
void aica_start(uint8_t chn);
void aica_stop(uint8_t chn);

void aica_update_channels(uint64_t mask);
void aica_start_channels(uint64_t mask);
void aica_stop_channels(uint64_t mask);

int8_t aica_reserve_channel(void);
void aica_unreserve_channel(uint8_t ch);

uint16_t aica_get_pos_unlocked(uint8_t chn);

#endif  /* __AICA_AICA_H */

