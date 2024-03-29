/* KallistiOS ##version##

   snd_sfxmgr.c
   Copyright (C) 2000, 2001, 2002, 2003, 2004 Megan Potter
   Copyright (C) 2023 Ruslan Rostovtsev
   Copyright (C) 2023 Andy Barajas

   Sound effects management system; this thing loads and plays sound effects
   during game operation.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>

#include <sys/queue.h>
#include <kos/fs.h>
#include <arch/irq.h>
#include <dc/spu.h>
#include <dc/sound/cmd_iface.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>

struct snd_effect;
LIST_HEAD(selist, snd_effect);

typedef struct snd_effect {
    uint32_t  locl, locr;
    uint32_t  len;
    uint32_t  rate;
    uint32_t  used;
    uint32_t  fmt;
    uint16_t  stereo;

    LIST_ENTRY(snd_effect)  list;
} snd_effect_t;

struct selist snd_effects;

/* The next channel we'll use to play sound effects. */
static int sfx_nextchan = 0;

/* Our channel-in-use mask. */
static uint64_t sfx_inuse = 0;

/* Unload all loaded samples and free their SPU RAM */
void snd_sfx_unload_all(void) {
    snd_effect_t *t, *n;

    t = LIST_FIRST(&snd_effects);

    while(t) {
        n = LIST_NEXT(t, list);

        snd_mem_free(t->locl);

        if(t->stereo)
            snd_mem_free(t->locr);

        free(t);

        t = n;
    }

    LIST_INIT(&snd_effects);
}

/* Unload a single sample */
void snd_sfx_unload(sfxhnd_t idx) {
    snd_effect_t *t = (snd_effect_t *)idx;

    if(idx == SFXHND_INVALID) {
        dbglog(DBG_WARNING, "snd_sfx: can't unload an invalid SFXHND\n");
        return;
    }

    snd_mem_free(t->locl);

    if(t->stereo)
        snd_mem_free(t->locr);

    LIST_REMOVE(t, list);
    free(t);
}

typedef struct __attribute__((__packed__)) {
    uint8_t riff[4];
    int32_t totalsize;
    uint8_t riff_format[4];
} wavmagic_t;

typedef struct __attribute__((__packed__)) {
    uint8_t id[4];
    size_t size;
} chunkhdr_t;

typedef struct __attribute__((__packed__)) {
    int16_t format;
    int16_t channels;
    int32_t sample_rate;
    int32_t byte_per_sec;
    int16_t blocksize;
    int16_t sample_size;
} fmthdr_t;

/* WAV header */
typedef struct __attribute__((__packed__)) {
    wavmagic_t magic;

    chunkhdr_t chunk;

    fmthdr_t fmt;
} wavhdr_t;

/* WAV sample formats */
#define WAVE_FMT_PCM                   0x0001 /* PCM */
#define WAVE_FMT_YAMAHA_ADPCM_ITU_G723 0x0014 /* ITU G.723 Yamaha ADPCM (KallistiOS) */
#define WAVE_FMT_YAMAHA_ADPCM          0x0020 /* Yamaha ADPCM (ffmpeg) */

static int read_wav_header(file_t fd, wavhdr_t *wavhdr) {
    if(fs_read(fd, &(wavhdr->magic), sizeof(wavhdr->magic)) != sizeof(wavhdr->magic)) {
        dbglog(DBG_WARNING, "snd_sfx: can't read wav header\n");
        return -1;
    }

    if(strncmp((const char*)wavhdr->magic.riff, "RIFF", 4)) {
        dbglog(DBG_WARNING, "snd_sfx: sfx file is not RIFF\n");
        return -1;
    }

    /* Check file magic */
    if(strncmp((const char*)wavhdr->magic.riff_format, "WAVE", 4)) {
        dbglog(DBG_WARNING, "snd_sfx: sfx file is not RIFF WAVE\n");
        return -1;
    }

    do {
        /* Read the chunk header */
        if(fs_read(fd, &(wavhdr->chunk), sizeof(wavhdr->chunk)) != sizeof(wavhdr->chunk)) {
            dbglog(DBG_WARNING, "snd_sfx: can't read chunk header\n");
            return -1;
        }

        /* If it is the fmt chunk, grab the fields we care about and skip the 
           rest of the section if there is more */
        if(strncmp((const char *)wavhdr->chunk.id, "fmt ", 4) == 0) {
            if(fs_read(fd, &(wavhdr->fmt), sizeof(wavhdr->fmt)) != sizeof(wavhdr->fmt)) {
                dbglog(DBG_WARNING, "snd_sfx: can't read fmt header\n");
                return -1;
            }

            /* Skip the rest of the fmt chunk */ 
            fs_seek(fd, wavhdr->chunk.size - sizeof(wavhdr->fmt), SEEK_CUR);
        }
        /* If we found the data chunk, we are done */
        else if(strncmp((const char *)wavhdr->chunk.id, "data", 4) == 0) {
            break;
        }
        /* Skip meta data */
        else { 
            fs_seek(fd, wavhdr->chunk.size, SEEK_CUR);
        }
    } while(1);

    return 0;
}

static uint8_t *read_wav_data(file_t fd, wavhdr_t *wavhdr) {
    /* Allocate memory for WAV data */
    uint8_t *wav_data = memalign(32, wavhdr->chunk.size);

    if(wav_data == NULL)
        return NULL;

    /* Read WAV data */
    if((size_t)fs_read(fd, wav_data, wavhdr->chunk.size) != wavhdr->chunk.size) {
        dbglog(DBG_WARNING, "snd_sfx: file has not been fully read.\n");
        free(wav_data);
        return NULL;
    }

    return wav_data;
}

static snd_effect_t *create_snd_effect(wavhdr_t *wavhdr, uint8_t *wav_data) {
    snd_effect_t *effect;
    uint32_t len, rate;
    uint16_t channels, bitsize, fmt;
    
    effect = malloc(sizeof(snd_effect_t));
    if(effect == NULL)
        return NULL;

    memset(effect, 0, sizeof(snd_effect_t));

    fmt = wavhdr->fmt.format;
    channels = wavhdr->fmt.channels;
    rate = wavhdr->fmt.sample_rate;
    bitsize = wavhdr->fmt.sample_size;
    len = wavhdr->chunk.size;

    effect->rate = rate;
    effect->stereo = channels > 1;

    if(channels == 1) {
        /* Mono PCM/ADPCM */
        if(fmt == WAVE_FMT_YAMAHA_ADPCM_ITU_G723 || fmt == WAVE_FMT_YAMAHA_ADPCM) {
            effect->fmt = AICA_SM_ADPCM;
            effect->len = len * 2; /* 4-bit packed samples */
        }
        else if(fmt == WAVE_FMT_PCM && bitsize == 8) {
            effect->fmt = AICA_SM_8BIT;
            effect->len = len;
        }
        else if(fmt == WAVE_FMT_PCM && bitsize == 16) {
            effect->fmt = AICA_SM_16BIT;
            effect->len = len / 2;
        }
        else {
            goto err_occurred;
        }

        effect->locl = snd_mem_malloc(len);
        if(effect->locl)
            spu_memload_sq(effect->locl, wav_data, len);

        effect->locr = 0;
    }
    else if(channels == 2 && fmt == WAVE_FMT_PCM && bitsize == 16) {
        /* Stereo 16-bit PCM */
        effect->len = len / 4; /* Two stereo, 16-bit samples */
        effect->fmt = AICA_SM_16BIT;
        effect->locl = snd_mem_malloc(len / 2);
        effect->locr = snd_mem_malloc(len / 2);

        if(effect->locl && effect->locr)
            snd_pcm16_split_sq((uint32_t *)wav_data, effect->locl, effect->locr, len);
    }
    else if(channels == 2 && fmt == WAVE_FMT_PCM && bitsize == 8) {
        /* Stereo 8-bit PCM */
        uint32_t *left_buf = memalign(32, len / 2), *right_buf;

        if(left_buf == NULL)
            goto err_occurred;

        right_buf = memalign(32, len / 2);
        if(right_buf == NULL) {
            free(left_buf);
            goto err_occurred;
        }

        snd_pcm8_split((uint32_t *)wav_data, left_buf, right_buf, len);

        effect->fmt = AICA_SM_8BIT;
        effect->len = len / 2;
        effect->locl = snd_mem_malloc(len / 2);
        effect->locr = snd_mem_malloc(len / 2);

        if(effect->locl)
            spu_memload_sq(effect->locl, left_buf, len / 2);

        if(effect->locr)
            spu_memload_sq(effect->locr, right_buf, len / 2);

        free(left_buf);
        free(right_buf);
    }
    else if(channels == 2 && fmt == WAVE_FMT_YAMAHA_ADPCM_ITU_G723) {
        /* Stereo ADPCM ITU G.723 (channels are not interleaved) */
        uint8_t *right_buf = wav_data + (len / 2);
        int ownmem = 0;

        if(((uintptr_t)right_buf) & 3) {
            right_buf = (uint8_t *)memalign(32, len / 2);

            if(right_buf == NULL)
                goto err_occurred;

            ownmem = 1;
            memcpy(right_buf, wav_data + (len / 2), len / 2);
        }

        effect->len = len;   /* Two stereo, 4-bit samples */
        effect->fmt = AICA_SM_ADPCM;
        effect->locl = snd_mem_malloc(len / 2);
        effect->locr = snd_mem_malloc(len / 2);

        if(effect->locl)
            spu_memload_sq(effect->locl, wav_data, len / 2);

        if(effect->locr)
            spu_memload_sq(effect->locr, right_buf, len / 2);

        if(ownmem)
            free(right_buf);
    }
    else if(channels == 2 && fmt == WAVE_FMT_YAMAHA_ADPCM) {
        /* Stereo Yamaha ADPCM (channels are interleaved) */
        uint32_t *left_buf = (uint32_t *)memalign(32, len / 2), *right_buf;

        if(left_buf == NULL)
            goto err_occurred;


        right_buf = (uint32_t *)memalign(32, len / 2);

        if(right_buf == NULL) {
            free(left_buf);
            goto err_occurred;
        }

        snd_adpcm_split((uint32_t *)wav_data, left_buf, right_buf, len);

        effect->len = len; /* Two stereo, 4-bit samples */
        effect->fmt = AICA_SM_ADPCM;
        effect->locl = snd_mem_malloc(len / 2);
        effect->locr = snd_mem_malloc(len / 2);

        if(effect->locl)
            spu_memload_sq(effect->locl, left_buf, len / 2);

        if(effect->locr)
            spu_memload_sq(effect->locr, right_buf, len / 2);

        free(left_buf);
        free(right_buf);
    }
    else {
err_occurred:
        free(effect);
        effect = SFXHND_INVALID;
    }

    return effect;
}

/* Load a sound effect from a WAV file and return a handle to it */
sfxhnd_t snd_sfx_load(const char *fn) {
    file_t fd;
    wavhdr_t wavhdr;
    snd_effect_t *effect;
    uint8_t *wav_data;
    uint32_t sample_count;

    dbglog(DBG_DEBUG, "snd_sfx: loading effect %s\n", fn);

    /* Open the sound effect file */
    fd = fs_open(fn, O_RDONLY);
    if(fd <= FILEHND_INVALID) {
        dbglog(DBG_WARNING, "snd_sfx: can't open sfx %s\n", fn);
        return SFXHND_INVALID;
    }

    /* Read WAV header */
    if(read_wav_header(fd, &wavhdr) < 0) {
        fs_close(fd);
        return SFXHND_INVALID;
    }

    dbglog(DBG_DEBUG, "WAVE file is %s, %luHZ, %d bits/sample, "
        "%u bytes total, format %d\n", 
           wavhdr.fmt.channels == 1 ? "mono" : "stereo", 
           wavhdr.fmt.sample_rate, 
           wavhdr.fmt.sample_size, 
           wavhdr.chunk.size, 
           wavhdr.fmt.format);

    sample_count = wavhdr.fmt.sample_size >= 8 
        ? wavhdr.chunk.size / ((wavhdr.fmt.sample_size / 8) * wavhdr.fmt.channels) 
        : wavhdr.chunk.size / (0.5 * wavhdr.fmt.channels);

    if(sample_count > 65534) {
        dbglog(DBG_WARNING, "WAVE file is over 65534 samples\n");
    }

    /* Read WAV data */
    wav_data = read_wav_data(fd, &wavhdr);
    fs_close(fd);
    if(!wav_data)
        return SFXHND_INVALID;

    /* Create and initialize sound effect */
    effect = create_snd_effect(&wavhdr, wav_data);
    if(!effect) {
        free(wav_data);
        return SFXHND_INVALID;
    }

    /* Finish up and return the sound effect handle */
    free(wav_data);
    LIST_INSERT_HEAD(&snd_effects, effect, list);

    return (sfxhnd_t)effect;
}

int snd_sfx_play_chn(int chn, sfxhnd_t idx, int vol, int pan) {
    int size;
    snd_effect_t *t = (snd_effect_t *)idx;
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    size = t->len;

    if(size >= 65535) size = 65534;

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_START;
    chan->base = t->locl;
    chan->type = t->fmt;
    chan->length = size;
    chan->loop = 0;
    chan->loopstart = 0;
    chan->loopend = size;
    chan->freq = t->rate;
    chan->vol = vol;

    if(!t->stereo) {
        chan->pan = pan;
        snd_sh4_to_aica(tmp, cmd->size);
    }
    else {
        chan->pan = 0;

        snd_sh4_to_aica_stop();
        snd_sh4_to_aica(tmp, cmd->size);

        cmd->cmd_id = chn + 1;
        chan->base = t->locr;
        chan->pan = 255;
        snd_sh4_to_aica(tmp, cmd->size);
        snd_sh4_to_aica_start();
    }

    return chn;
}

int snd_sfx_play(sfxhnd_t idx, int vol, int pan) {
    int chn, moved, old;

    /* This isn't perfect.. but it should be good enough. */
    old = irq_disable();
    chn = sfx_nextchan;
    moved = 0;

    while(sfx_inuse & (1 << chn)) {
        chn = (chn + 1) % 64;

        if(sfx_nextchan == chn)
            break;

        moved++;
    }

    irq_restore(old);

    if(moved && chn == sfx_nextchan) {
        return -1;
    }
    else {
        sfx_nextchan = (chn + 2) % 64;  /* in case of stereo */
        return snd_sfx_play_chn(chn, idx, vol, pan);
    }
}

void snd_sfx_stop(int chn) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_STOP;
    chan->base = 0;
    chan->type = 0;
    chan->length = 0;
    chan->loop = 0;
    chan->loopstart = 0;
    chan->loopend = 0;
    chan->freq = 44100;
    chan->vol = 0;
    chan->pan = 0;
    snd_sh4_to_aica(tmp, cmd->size);
}

void snd_sfx_stop_all(void) {
    int i;

    for(i = 0; i < 64; i++) {
        if(sfx_inuse & (1 << i))
            continue;

        snd_sfx_stop(i);
    }
}

int snd_sfx_chn_alloc(void) {
    aica_cmd_t cmd = {
        .size = sizeof(cmd) / 4,
        .cmd = AICA_CMD_RESERVE,
        .misc[0] = (unsigned int)-1,
    };
    int chn, old;

    chn = snd_sh4_to_aica_with_response(&cmd);

    old = irq_disable();
    sfx_inuse |= 1 << chn;
    irq_restore(old);

    return chn;
}

void snd_sfx_chn_free(int chn) {
    aica_cmd_t cmd = {
        .size = sizeof(cmd) / 4,
        .cmd = AICA_CMD_RESERVE,
        .misc[0] = chn,
    };
    int old;

    old = irq_disable();
    sfx_inuse &= ~(1 << chn);
    irq_restore(old);

    snd_sh4_to_aica(&cmd, sizeof(cmd) / 4);
}
