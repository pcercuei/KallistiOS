#ifndef __AICA_H
#define __AICA_H

#include <stdint.h>

/* volatile unsigned char *dc_snd_base = (unsigned char *)0x00800000; */
#define dc_snd_base ((volatile uint8_t *)0x00800000)

/* Some convenience macros */
#define SNDREG32A(x) ((volatile uint32_t *)(dc_snd_base + (x)))
#define SNDREG32(x) (*SNDREG32A(x))
#define SNDREG8A(x) (dc_snd_base + (x))
#define SNDREG8(x) (*SNDREG8A(x))
#define CHNREG32A(chn, x) SNDREG32A(0x80*(chn) + (x))
#define CHNREG32(chn, x) (*CHNREG32A(chn, x))
#define CHNREG8A(chn, x) SNDREG8A(0x80*(chn) + (x))
#define CHNREG8(chn, x) (*CHNREG8A(chn, x))

/* flags for aica_play() */
#define AICA_PLAY_DELAY 0x1
#define AICA_PLAY_LOOP  0x2

void aica_init(void);
void aica_play(uint8_t ch, void *data, uint32_t mode,
               uint32_t start, uint32_t end, uint32_t freq,
               uint8_t vol, uint8_t pan, uint32_t flags);
void aica_sync_play(uint64_t chmap);
void aica_stop(uint8_t ch);
void aica_vol(uint8_t ch, uint8_t vol);
void aica_pan(uint8_t ch, uint8_t pan);
void aica_freq(uint8_t ch, uint32_t freq);
int aica_get_pos(uint8_t ch);

#endif  /* __AICA_H */

