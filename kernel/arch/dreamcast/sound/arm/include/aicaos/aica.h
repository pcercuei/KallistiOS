#ifndef __AICA_H
#define __AICA_H

/* flags for aica_play() */
#define AICA_PLAY_DELAY 0x1
#define AICA_PLAY_LOOP  0x2

void aica_init(void);
void aica_play(unsigned char ch, void *data, unsigned int mode,
               unsigned int start, unsigned int end, unsigned int freq,
               unsigned char vol, unsigned char pan, unsigned int flags);
void aica_sync_play(unsigned long long chmap);
void aica_stop(unsigned char ch);
void aica_vol(unsigned char ch, unsigned char vol);
void aica_pan(unsigned char ch, unsigned char pan);
void aica_freq(unsigned char ch, unsigned int freq);
int aica_get_pos(unsigned char ch);

#endif  /* __AICA_H */

