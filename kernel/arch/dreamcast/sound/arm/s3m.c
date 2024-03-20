/* KallistiOS ##version##

   s3m.c
   Copyright (C) 2024 Paul Cercueil

   S3M player
*/

#include "aica_comm.h"
#include "aica.h"
#include "mm.h"
#include "queue.h"
#include "task.h"

enum header_flags {
    HEADER_ST2_VIBRATO  = 0x01,
    HEADER_ST2_TEMPO    = 0x02,
    HEADER_AMIGA_SLIDES = 0x04,
    HEADER_0VOL_OPT     = 0x08,
    HEADER_AMIGA_LIMIT  = 0x10,
    HEADER_SOUNDBLASTER = 0x20,
    HEADER_ST3_VOLSLIDE = 0x40,
    HEADER_HAS_SPECIAL  = 0x80,
};

enum channel_settings_flags {
    CH_SETTINGS_DIS     = 0x80,
};

enum instrument_type {
    INSTRUMENT_EMPTY    = 0x00,
    INSTRUMENT_PCM      = 0x01,
};

enum instrument_flags {
    INSTRUMENT_LOOP     = 0x1,
    INSTRUMENT_STEREO   = 0x2,
    INSTRUMENT_16BIT    = 0x4,
};

enum sample_type {
    SAMPLE_TYPE_SIGNED  = 0x1,
    SAMPLE_TYPE_UNSIG   = 0x2,
};

struct s3m_header {
    char title[28];
    char signature1;
    char type;
    char _resv[2];

    unsigned short nb_orders;
    unsigned short nb_instruments;
    unsigned short nb_patterns;
    unsigned short flags;
    unsigned short tracker_version;
    unsigned short sample_type;
    unsigned int signature2;

    unsigned char global_volume;
    unsigned char initial_speed;
    unsigned char initial_tempo;
    unsigned char master_volume;
    unsigned char ultra_click;
    unsigned char default_panning;
    unsigned char _resv2[2];
    unsigned int _resv3;
    unsigned char _resv4[2];
    short special;

    unsigned char channel_settings[32];
};

_Static_assert(sizeof(struct s3m_header) == 0x60, "Wrong header size");

struct instrument_header {
    char type;
    char filename[12];
    unsigned char data_offt_hi;
    unsigned short data_offt_lo;

    unsigned int length;
    unsigned int loop_start;
    unsigned int loop_end;
    char volume;
    char _resv;
    char packing;
    char flags;

    unsigned int c2spd;
    unsigned int _resv2[3];

    char sample_name[28];
    unsigned int signature;
};

struct channel_action {
    unsigned char note;
    unsigned char instrument;
    unsigned char volume;
    unsigned char special_cmd;
    unsigned char cmd_info;
    unsigned char _resv[3];
};

struct channel_state {
    int volume;
    unsigned int note_freq;
    unsigned int vibrato_idx;
    unsigned int last_vibrato;
    unsigned int current_note_freq;
    unsigned int previous_note_freq;
    unsigned int portamento;
    unsigned char panning;
};

struct s3m_state {
    struct s3m_header *header;
    struct instrument_header *current_instrument;

    struct task task;
    unsigned int stack[0x2000];

    unsigned int order;
    unsigned int tempo;
    unsigned int speed;
    unsigned int row;
    unsigned int period;
    unsigned int next_row;
    unsigned int next_sleep_ms;

    int pattern_break;
    int order_break;

    char stop;
    char playing;
    unsigned int row_increment;
    unsigned short cumulative_volume_modifier;
    unsigned short volume_up_modifier;
    unsigned short volume_down_modifier;

    const unsigned char *packet_ptr;

    unsigned char channels[32];
    struct channel_action actions[32];
    struct channel_state states[32];
};

static const unsigned short periods[] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
};

/* Sinusoidal vibration table for vibrato effect */
static const short vibration_table[64] =
{
       0,   24,   49,   74,   97,  120,  141,  161,
     180,  197,  212,  224,  235,  244,  250,  253,
     255,  253,  250,  244,  235,  224,  212,  197,
     180,  161,  141,  120,   97,   74,   49,   24,
       0,  -24,  -49,  -74,  -97, -120, -141, -161,
    -180, -197, -212, -224, -235, -244, -250, -253,
    -255, -253, -250, -244, -235, -224, -212, -197,
    -180, -161, -141, -120,  -97,  -74,  -49,  -24,
};

static inline char get_order(struct s3m_header *header, unsigned int order)
{
    return ((char *)(header + 1))[order];
}

static inline unsigned short *instrument_pptrs(struct s3m_header *header)
{
    return (unsigned short *)
        ((unsigned int)(header + 1) + header->nb_orders);
}

static inline struct instrument_header *
get_instrument(struct s3m_header *header, unsigned int instrument)
{
    unsigned short *offsets = instrument_pptrs(header);

    return (struct instrument_header *)
        ((unsigned int)header + offsets[instrument] * 16);
}

static inline unsigned short *pattern_pptrs(struct s3m_header *header)
{
    return instrument_pptrs(header) + header->nb_instruments;
}

static inline char *panning_list(struct s3m_header *header)
{
    return (char *)(pattern_pptrs(header) + header->nb_patterns);
}

static inline void *samples_ptr(struct s3m_header *header,
                                struct instrument_header *iheader)
{
    return (void *)((unsigned int)header
                    + ((unsigned int)iheader->data_offt_lo << 4)
                    + ((unsigned int)iheader->data_offt_hi << 20));
}

static void compute_period(struct s3m_state *state, unsigned char tempo)
{
    state->period = (125 * 20) / tempo;
}

static void reset_tempo_and_speed(struct s3m_state *state)
{
    if (state->tempo != state->header->initial_tempo)
        compute_period(state, state->header->initial_tempo);

    state->tempo = state->header->initial_tempo;
    state->speed = state->header->initial_speed;
}

static void set_tempo(struct s3m_state *state, unsigned char cmd_info)
{
    if (state->row_increment == 0) {
        switch (cmd_info >> 4) {
        case 0:
            if (state->tempo > cmd_info)
                state->tempo -= cmd_info;
            else
                state->tempo = 1;
            break;

        case 1:
            state->tempo += cmd_info & 0xf;
            break;
        default:
            state->tempo = cmd_info;
            break;
        }

        compute_period(state, state->tempo);
    }
}

static void set_speed(struct s3m_state *state, unsigned char speed)
{
    state->speed = speed;
}

static void set_order_position(struct s3m_state *state,
                               unsigned int order, unsigned int row)
{
    struct s3m_header *header = state->header;
    unsigned int i, offset, pattern_offset;
    unsigned char panning;

    if (order >= header->nb_orders) {
        order = 0;
        row = 0;
    }

    if (get_order(header, order) == 0xff) {
        /* End of song */
        order = 0;
    }

    while (get_order(header, order) == 0xfe) {
        /* Marker pattern */
        order++;
    }

    state->order = order;
    state->next_row = row;
    state->row = 0;
    state->row_increment = 0;

    offset = get_order(header, order);
    if (offset >= header->nb_patterns) {
        aica_printf("Invalid order %u\n", offset);
        return;
    }

    pattern_offset = pattern_pptrs(header)[offset];
    state->packet_ptr = (const unsigned char *)
        ((unsigned int)header + pattern_offset * 16 + 2);

    if (order == 0) {
        /* First order: reset to default values */
        reset_tempo_and_speed(state);

        for (i = 0; i < 32; i++) {
            state->states[i].volume = 0x3f;

            if (header->default_panning == 252
                && (panning_list(header)[i] & 0x20))
                panning = panning_list(header)[i] << 4;
            else
                panning = (header->channel_settings[i] & 0xf) < 8 ? 0x30 : 0xc0;

            state->states[i].panning = panning;
        }
    }
}

static void reset_volume_modifiers(struct s3m_state *state)
{
    state->volume_up_modifier = 0;
    state->volume_down_modifier = 0;
    state->cumulative_volume_modifier = 0;
    state->header->global_volume = 0x40;
}

static void process_volume_modifiers(struct s3m_state *state)
{
    if (state->volume_up_modifier) {
        state->cumulative_volume_modifier += state->volume_up_modifier;

        if (state->cumulative_volume_modifier >= 0x4000)
            reset_volume_modifiers(state);
        else
            state->header->global_volume = state->cumulative_volume_modifier >> 8;

    } else if (state->volume_down_modifier) {
        state->cumulative_volume_modifier += state->volume_down_modifier;

        if (state->cumulative_volume_modifier >= 0x4000)
            reset_volume_modifiers(state);
        else
            state->header->global_volume = 0x40 - (state->cumulative_volume_modifier >> 8);
    }
}

static unsigned int compute_freq(struct instrument_header *instrument, unsigned char note)
{
    unsigned int *frequencies = (unsigned int *)instrument->_resv2[0];
    unsigned int freq, note_shift = note >> 4;

    freq = frequencies[note & 0xf];

    if (note_shift > 11)
        freq <<= note_shift - 11;
    else
        freq >>= 11 - note_shift;

    return freq;
}

static void process_row(struct s3m_state *state)
{
    struct s3m_header *header = state->header;
    struct channel_action *actions = state->actions;
    struct instrument_header *instrument;
    unsigned int i, chid, volume, nmode, loop_start, loop_end, flags = 0;
    unsigned char action, ch;
    void *samples;

    memset(actions, 0, sizeof(*actions) * 32);

    for (;;) {
        action = *state->packet_ptr++;

        if (action == 0)
            break;

        chid = action & 0x1f;

        if (action & 0x20) {
            actions[chid].note = *state->packet_ptr++;
            actions[chid].instrument = *state->packet_ptr++;
        }

        if (action & 0x40)
            actions[chid].volume = *state->packet_ptr++;

        if (action & 0x80) {
            actions[chid].special_cmd = *state->packet_ptr++;
            actions[chid].cmd_info = *state->packet_ptr++;
        }
    }

    for (i = 0; i < 32; i++) {
        if (!actions[i].note)
            continue;

        if (header->channel_settings[i] & CH_SETTINGS_DIS)
            continue;

        ch = state->channels[i];

        if (actions[i].note == 254) {
            aica_stop(ch);
            continue;
        }

        /* TODO: Why -1? */
        instrument = get_instrument(header, actions[i].instrument - 1);
        state->current_instrument = instrument;

        state->states[i].note_freq = compute_freq(instrument, actions[i].note);

        state->states[i].vibrato_idx = 0;
        state->states[i].previous_note_freq = state->states[i].current_note_freq;
        state->states[i].current_note_freq = state->states[i].note_freq;

        if (actions[i].volume)
            state->states[i].volume = actions[i].volume;
#if 0
        else
            state->states[i].volume = instrument->volume;

        volume = (state->states[i].volume
                  * (unsigned int)header->global_volume) >> 4;
#else
        else
            state->states[i].volume = 0x3f;

	state->states[i].volume = 0x3f;

        volume = (state->states[i].volume
                  * (unsigned int)instrument->volume
                  * (unsigned int)header->global_volume) >> 10;
#endif

        samples = samples_ptr(header, instrument);

        if (instrument->flags & INSTRUMENT_LOOP) {
            loop_end = instrument->loop_end;
            flags |= AICA_PLAY_LOOP;
        } else {
            loop_end = instrument->length;
        }

        loop_start = instrument->loop_start;

        if (instrument->packing) {
            nmode = AICA_SM_ADPCM;
            loop_start *= 2;
            loop_end *= 2;
        } else if (instrument->flags & INSTRUMENT_16BIT) {
            nmode = AICA_SM_16BIT;
            loop_start /= 2;
            loop_end /= 2;
        }
        else {
            nmode = AICA_SM_8BIT;
        }

        aica_play(ch,           /* channel */
                  samples,      /* pointer to samples */
                  nmode,        /* samples compression mode */
                  loop_start,
                  loop_end,
                  state->states[i].note_freq,
                  volume,
                  state->states[i].panning,
                  flags);
    }
}

static void volume_slide(struct s3m_state *state, unsigned char cmd_info, unsigned int i)
{
    unsigned int volume;

    if ((cmd_info & 0xf0) == 0xf0) {
        /* Fine volume slide down */
        if (state->row_increment == 0) {
            state->states[i].volume -= cmd_info & 0x0f;

            if (state->states[i].volume < 0)
                state->states[i].volume = 0;
        }
    } else if ((cmd_info & 0x0f) == 0x0f) {
        /* Fine volume slide up */
        if (state->row_increment == 0) {
            state->states[i].volume += cmd_info >> 4;

            if (state->states[i].volume > 63)
                state->states[i].volume = 63;
        }
    } else if (cmd_info & 0x0f) {
        /* Volume slide down */
        state->states[i].volume -= cmd_info & 0x0f;

        if (state->states[i].volume < 0)
            state->states[i].volume = 0;
    } else {
        /* Volume slide up */
        state->states[i].volume += cmd_info >> 4;

        if (state->states[i].volume > 63)
            state->states[i].volume = 63;
    }

    volume = (state->states[i].volume
              * state->header->global_volume/*
              * state->current_instrument->volume*/) >> 10;

    //aica_vol(state->channels[i], volume);
}

static void portamento(struct s3m_state *state,
                       unsigned char cmd_info, int i, int down)
{
    if (state->row_increment == 0) {
        switch (cmd_info & 0xf0) {
        case 0xf0:
            /* Fine slide */
            state->states[i].portamento = ((unsigned int)cmd_info & 0x0f) << 2;
            break;

        case 0xe0:
            /* Extra-fine slide */
            state->states[i].portamento = (unsigned int)cmd_info & 0x0f;
            break;

        default:
            /* Regular slide */
            state->states[i].portamento = (unsigned int)cmd_info & 0x0f;
            break;
        }
    }

    if (down)
        state->states[i].current_note_freq -= state->states[i].portamento;
    else
        state->states[i].current_note_freq += state->states[i].portamento;

    aica_freq(state->channels[i], state->states[i].previous_note_freq);
}

static void tone_portamento(struct s3m_state *state,
                            unsigned char cmd_info, unsigned int i)
{
    struct channel_state *cstate = &state->states[i];

    if (cstate->previous_note_freq < cstate->current_note_freq) {
        /* Up portamento */
        cstate->previous_note_freq += (unsigned int)cmd_info << 4;

        if (cstate->previous_note_freq > cstate->current_note_freq)
            cstate->previous_note_freq = cstate->current_note_freq;

        aica_freq(state->channels[i], cstate->previous_note_freq);

    } else if (cstate->previous_note_freq > cstate->current_note_freq) {
        /* Down portamento */
        cstate->previous_note_freq -= (unsigned int)cmd_info << 4;

        if (cstate->previous_note_freq < cstate->current_note_freq)
            cstate->previous_note_freq = cstate->current_note_freq;

        aica_freq(state->channels[i], cstate->previous_note_freq);
    }
}

static void vibrato(struct s3m_state *state,
                    unsigned char cmd_info, unsigned int i)
{
    struct channel_state *cstate = &state->states[i];
    unsigned int vibrato_idx;
    int tmp;

    vibrato_idx = (cstate->vibrato_idx >> 2) & 0x3f;
    tmp = vibration_table[vibrato_idx];

    tmp = (tmp * (int)(cmd_info & 0xf)) >> 4;

    aica_freq(state->channels[i], cstate->note_freq + tmp);

    cstate->vibrato_idx += (cmd_info & 0xf0) >> 2;
}

static void process_row_effects(struct s3m_state *state)
{
    struct channel_action *action;
    unsigned int i, tmp;

    for (i = 0; i < 32; i++) {
        action = &state->actions[i];

        switch (action->special_cmd) {

        case 'A' - 'A' + 1:
            if (state->row_increment == 0 && action->cmd_info)
                set_speed(state, action->cmd_info);
            break;

        case 'B' - 'A' + 1:
            state->order_break = action->cmd_info;
            break;

        case 'C' - 'A' + 1:
#if 0
            if (state->row_increment == 0) {
                tmp = (action->cmd_info >> 4) * 10 + (action->cmd_info & 0xf);
                if (tmp < 64)
                    set_order_position(state, state->order + 1, tmp);
            }
#else
            state->pattern_break = action->cmd_info;
#endif
            break;

        case 'D' - 'A' + 1:
            volume_slide(state, action->cmd_info, i);
            break;

        case 'E' - 'A' + 1:
            portamento(state, action->cmd_info, i, 1);
            break;

        case 'F' - 'A' + 1:
            portamento(state, action->cmd_info, i, 0);
            break;

        case 'G' - 'A' + 1:
            tone_portamento(state, action->cmd_info, i);
            break;

        case 'H' - 'A' + 1:
            state->states[i].last_vibrato = action->cmd_info;
            vibrato(state, action->cmd_info, i);
            break;

        case 'K' - 'A' + 1:
            vibrato(state, state->states[i].last_vibrato, i);
            volume_slide(state, action->cmd_info, i);
            break;

        case 'T' - 'A' + 1:
            set_tempo(state, action->cmd_info);
            break;

        default:
            break;
        }
    }
}

static void s3m_reserve_channels(struct s3m_state *state)
{
    const struct s3m_header *header = state->header;
    unsigned int i;

    for (i = 0; i < 32; i++)
        if (!(header->channel_settings[i] & CH_SETTINGS_DIS))
            state->channels[i] = aica_reserve_channel();
}

static void s3m_unreserve_channels(struct s3m_state *state)
{
    const struct s3m_header *header = state->header;
    unsigned int i;

    for (i = 0; i < 32; i++) {
        if (!(header->channel_settings[i] & CH_SETTINGS_DIS)) {
            aica_stop(state->channels[i]);
            aica_unreserve_channel(state->channels[i]);
        }
    }
}

static void
instrument_sign_samples(struct instrument_header *header, void *samples)
{
    unsigned int nb = header->length;

    if (header->flags & INSTRUMENT_16BIT) {
        short *ptr = samples;

        while (nb--)
            *ptr++ ^= 0x8000;
    } else {
        char *ptr = samples;

        while (nb--)
            *ptr++ ^= 0x80;
    }

    if (header->packing)
        aica_printf("Samples are packed!\n");
}

static void sign_samples(struct s3m_header *header)
{
    struct instrument_header *instrument;
    unsigned int i;
    void *samples;

    for (i = 0; i < header->nb_instruments; i++) {
        instrument = get_instrument(header, i);

        if (instrument->type == INSTRUMENT_PCM) {
            samples = samples_ptr(header, instrument);
            instrument_sign_samples(instrument, samples);
        }
    }
}

static void precompute_instruments(struct s3m_state *state)
{
    struct s3m_header *header = state->header;
    struct instrument_header *instrument;
    unsigned int i, j, base, *ptr;
    void *samples;

    /* Precompute the instruments' frequencies for each note.
     * Then compute_freq() will only have to load a precomputed value and do
     * some bit-shifts. */

    for (i = 0; i < header->nb_instruments; i++) {
        instrument = get_instrument(header, i);
        if (instrument->type != 1)
            continue;

        base = 109565 * instrument->c2spd;

        ptr = aica_malloc(12 * sizeof(*ptr));
        instrument->_resv2[0] = (unsigned int)ptr;

        for (j = 0; j < 12; j++)
            ptr[j] = base / periods[j];
    }
}

static void s3m_free_instruments(struct s3m_state *state)
{
    struct s3m_header *header = state->header;
    struct instrument_header *instrument;
    unsigned int i;

    for (i = 0; i < header->nb_instruments; i++) {
        instrument = get_instrument(header, i);

        if (instrument->type == 1)
            aica_free((void *)instrument->_resv2[0]);
    }
}

static void s3m_run(struct s3m_header *header, struct s3m_state *state)
{
    //aica_printf("Starting playing S3M file.\n");

    aica_printf("Start S3M with header=0x%x state=0x%x\n",
		(unsigned int)header, (unsigned int)state);

    reset_volume_modifiers(state);
    set_order_position(state, 0, 0);

    while (!state->stop) {
        if (state->next_sleep_ms)
            task_sleep(ms_to_ticks(state->next_sleep_ms));

        process_volume_modifiers(state);

        if (state->row_increment == 0)
            process_row(state);

        process_row_effects(state);

        state->next_sleep_ms = state->period;
        state->row_increment++;

        if (state->row_increment >= state->speed) {
            if (state->row < 63) {
                state->row++;
            } else if (state->order_break != -1) {
                set_order_position(state, state->order_break, 0);
                state->order_break = -1;
            } else {
                set_order_position(state, state->order + 1, 0);
            }

            state->row_increment = 0;
        }
    }
}

void s3m_play(struct s3m_header *header)
{
    struct s3m_state *state;
    unsigned int params[4] = { (unsigned int)header, };

    if (header->signature1 != 0x1a || header->type != 0x10
        || header->signature2 != 0x4d524353)
    {
        aica_printf("Invalid file signature.\n");
        return;
    }

    state = aica_malloc(sizeof(*state));
    if (!state)
        return;

    memset(state, 0, sizeof(*state));

    state->header = header;
    state->pattern_break = -1;
    state->order_break = -1;

    /* Keep a pointer to the state in the header struct */
    header->_resv3 = (unsigned int)state;

    params[1] = (unsigned int)state;

    /* Pre-process samples if needed */
    if (header->sample_type == SAMPLE_TYPE_UNSIG)
        sign_samples(header);

    precompute_instruments(state);
    s3m_reserve_channels(state);

    task_init(&state->task, s3m_run, params,
              TASK_PRIO_NORMAL, state->stack, sizeof(state->stack));
}

void s3m_stop(struct s3m_header *header)
{
    struct s3m_state *state = (struct s3m_state *)header->_resv3;

    state->stop = 1;

    task_join(&state->task);

    s3m_free_instruments(state);
    s3m_unreserve_channels(state);
    aica_free(state);
}

void * memset(void *dest, int c, unsigned int len)
{
    char *dst = dest;

    while (len--)
        *dst++ = (char)c;

    return dest;
}
