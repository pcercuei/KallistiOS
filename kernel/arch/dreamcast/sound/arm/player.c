#include <aica_comm.h>
#include <aicaos/aica.h>
#include <aicaos/task.h>
#include <mikmod.h>
#include <mikmod_internals.h>
#include <stddef.h>
#include <stdio.h>

#include "player.h"

#define ARRAY_SIZE(x) (sizeof(x) ? sizeof(x) / sizeof((x)[0]) : 0)

#define NB_CHANNELS_MAX 32

#define DEBUG 0

static MODULE *module;
static void *buffer;

static struct task task;
static unsigned int stack[0x2000];

static volatile unsigned int stopped;

struct aica_sample {
    struct SAMPLE *mikmod_sample;
    unsigned short data[];
};

static struct aica_sample * sample_array[128];
static unsigned short sample_counter;

void s3m_play(struct s3m_header *header, size_t len)
{
    buffer = header;

    printf("MikMod init...\n");
    MikMod_Init("");

#if 0
    MikMod_RegisterAllLoaders();
#else
    _mm_registerloader(&load_s3m);
#endif

    printf("Loading module...\n");
    module = Player_LoadMem(buffer, len, NB_CHANNELS_MAX, 0);
    if (!module) {
        printf("Unable to initialize module: %d\n", MikMod_errno);
        return;
    }

    printf("Module loaded. Starting playback...\n");
    Player_Start(module);

    printf("Playback started!\n");
}

void s3m_stop(struct s3m_header *header)
{
    Player_Stop();
    Player_Free(module);

    MikMod_Exit();
}

static unsigned int current_tick;

static void aica_run_mikmod(void)
{
    while (!stopped) {
        if (0)
            printf("Handle tick %u/%u bpm %u\n", module->vbtick, module->sngspd, module->bpm);
        Player_HandleTick();

        task_sleep(ms_to_ticks(60000 / ((volatile UWORD)module->bpm * (volatile UWORD)module->sngspd * 6)));
    }
}

static BOOL aica_present(void)
{
    return 1;
}

static unsigned long aica_get_sample_length(int type, struct SAMPLE *s)
{
    return (s->length << !!(s->flags & SF_16BITS)) >> !!(s->flags & SF_ADPCM4);
}

static SWORD aica_load_sample(struct SAMPLOAD *s, int type)
{
    unsigned short idx = sample_counter++;
    struct aica_sample *sample;
    unsigned long len = aica_get_sample_length(type, s->sample);

    SL_SampleSigned(s);

    sample = malloc(sizeof(*sample) + len);
    if (!sample) {
        _mm_errno = MMERR_OUT_OF_MEMORY;
        return -1;
    }

    sample->mikmod_sample = s->sample;

    if (SL_Load(sample->data, s, len)) {
        free(sample);
        _mm_errno = MMERR_OUT_OF_MEMORY;
        return -1;
    }

    printf("Loaded sample %u.\n", idx);
    sample_array[idx] = sample;

    return (SWORD)idx;
}

static void aica_unload_sample(SWORD idx)
{
    struct aica_sample *sample = sample_array[idx];

    free(sample);
}

static unsigned long aica_get_free_space(int type)
{
    return mem_available();
}

static int aica_init_player(void)
{
    return 0;
}

static void aica_exit_player(void)
{
}

static int aica_reset_player(void)
{
    aica_exit_player();

    return aica_init_player();
}

static int aica_set_num_voices(void)
{
    return 0;
}

static int aica_start_player(void)
{
    stopped = 0;

    printf("Start player.\n");

    task_init(&task, "mikmod", aica_run_mikmod, NULL,
              TASK_PRIO_NORMAL, stack, sizeof(stack));

    return 0;
}

static void aica_stop_player(void)
{
    stopped = 1;
    task_join(&task);
}

static void aica_update_player(void)
{
}

static void aica_pause_player(void)
{
}

static void aica_voice_set_volume(UBYTE voice, UWORD volume)
{

    if (volume)
        volume--;
    if (DEBUG)
        printf("aica_vol = %u\n", volume);
    aica_vol(62 - voice, volume);
}

static UWORD aica_voice_get_volume(UBYTE voice)
{
    return 0;
}

static void aica_voice_set_frequency(UBYTE voice, ULONG freq)
{
    if (DEBUG)
        printf("aica_freq = %u\n", freq);
    aica_freq(62 - voice, freq);
}

static ULONG aica_voice_get_frequency(UBYTE voice)
{
    return 0;
}

static void aica_voice_set_panning(UBYTE voice, ULONG panning)
{
    aica_pan(62 - voice, panning);
}

static ULONG aica_voice_get_panning(UBYTE voice)
{
}

static void aica_voice_play(UBYTE voice, SWORD idx, ULONG start,
                            ULONG length, ULONG loopstart, ULONG loopend,
                            UWORD flags)
{
    struct aica_sample *sample = sample_array[idx];
    struct SAMPLE *mmsample = sample->mikmod_sample;
    unsigned int aica_flags = 0;
    UWORD volume;
    int mode;

    if (!(flags & SF_LOOP)) {
        loopstart = start;
        loopend = length;
    } else {
        aica_flags |= AICA_PLAY_LOOP;
        loopend - 1;
    }

    if (flags & SF_16BITS)
        mode = AICA_SM_16BIT;
    else if (flags & SF_ADPCM4)
        mode = AICA_SM_ADPCM;
    else
        mode = AICA_SM_8BIT;

    if (loopstart == loopend)
        return;

    if (DEBUG)
        printf("aica_play! freq = %u Hz data = 0x%x\n",
               mmsample->speed, (unsigned int)sample->data);

    volume = mmsample->volume;
    if (volume)
        volume--;
    aica_play(62 - voice, sample->data, mode,
              loopstart, loopend, mmsample->speed, volume,
              mmsample->panning, aica_flags);
}

static void aica_voice_stop(UBYTE voice)
{
    aica_stop(62 - voice);
}

static BOOL aica_voice_stopped(UBYTE voice)
{
    return 0;
}

static SLONG aica_voice_get_position(UBYTE voice)
{
    return 0;
}

static ULONG aica_voice_get_real_volume(UBYTE voice)
{
    return 0;
}

MDRIVER drv_nos = {
    .Name = "AICA",
    .Version = "Dreamcast AICA driver",
    .Alias = "aica",

    .HardVoiceLimit = ARRAY_SIZE(sample_array),
    .SoftVoiceLimit = ARRAY_SIZE(sample_array),

    .IsPresent = aica_present,

    .SampleLoad = aica_load_sample,
    .SampleUnload = aica_unload_sample,

    .FreeSampleSpace = aica_get_free_space,
    .RealSampleLength = aica_get_sample_length,

    .Init = aica_init_player,
    .Exit = aica_exit_player,
    .Reset = aica_reset_player,

    .SetNumVoices = aica_set_num_voices,
    .PlayStart = aica_start_player,
    .PlayStop = aica_stop_player,
    .Update = aica_update_player,
    .Pause = aica_pause_player,

    .VoiceSetVolume = aica_voice_set_volume,
    .VoiceGetVolume = aica_voice_get_volume,

    .VoiceSetFrequency = aica_voice_set_frequency,
    .VoiceGetFrequency = aica_voice_get_frequency,
    .VoiceSetPanning = aica_voice_set_panning,
    .VoiceGetPanning = aica_voice_get_panning,

    .VoicePlay = aica_voice_play,
    .VoiceStop = aica_voice_stop,
    .VoiceStopped = aica_voice_stopped,

    .VoiceGetPosition = aica_voice_get_position,
    .VoiceRealVolume = aica_voice_get_real_volume,
};
