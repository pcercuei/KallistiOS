/* KallistiOS ##version##

   main.c
   (c)2000-2002 Megan Potter

   Generic sound driver with streaming capabilities

   This slightly more complicated version allows for sound effect channels,
   and full sampling rate, panning, and volume control for each.

*/

#include <cmd_iface.h>

#include <aicaos/aica.h>
#include <aicaos/queue.h>
#include <aicaos/task.h>
#include <stdio.h>
#include <stdlib.h>

#include "s3m.h"

static union {
    unsigned int _info_buf[0x400];
    struct aica_tasks_info info;
} tasks_info;

/****************** Main Program ************************************/

/* Process a CHAN command */
void process_chn(struct aica_header *header, aica_cmd_t *pkt, aica_channel_t *chndat) {
    uint32 cmd_id = pkt->cmd_id;
    struct aica_channel *chn = &header->channels[cmd_id];
    unsigned long long start_sync;
    unsigned int flags;

    switch(chndat->cmd & AICA_CH_CMD_MASK) {
        case AICA_CH_CMD_NONE:
            break;
        case AICA_CH_CMD_START:

            if(chndat->cmd & AICA_CH_START_SYNC) {
                start_sync = ((unsigned long long)pkt->misc[0] << 32) | cmd_id;
                aica_sync_play(start_sync);
            }
            else {
                *chn = *chndat;
                chn->pos = 0;
                flags = 0;

                if (chn->loop)
                    flags |= AICA_PLAY_LOOP;
                if (chndat->cmd & AICA_CH_START_DELAY)
                    flags |= AICA_PLAY_DELAY;

                aica_play(cmd_id, (void *)chn->base, chn->type,
                          chn->loopstart, chn->loopend, chn->freq,
                          chn->vol, chn->pan, flags);
            }

            break;
        case AICA_CH_CMD_STOP:
            aica_stop(cmd_id);
            break;
        case AICA_CH_CMD_UPDATE:

            if(chndat->cmd & AICA_CH_UPDATE_SET_FREQ) {
                chn->freq = chndat->freq;
                aica_freq(cmd_id, chn->freq);
            }

            if(chndat->cmd & AICA_CH_UPDATE_SET_VOL) {
                chn->vol = chndat->vol;
                aica_vol(cmd_id, chn->vol);
            }

            if(chndat->cmd & AICA_CH_UPDATE_SET_PAN) {
                chn->pan = chndat->pan;
                aica_pan(cmd_id, chn->pan);
            }

            break;
        default:
            /* error */
            break;
    }
}

static void process_mm(struct aica_header *header,
                       uint32 cmd, uint32 arg0, uint32 arg1)
{
    unsigned int resp = 0;

    switch (cmd & AICA_CMD_MM_MASK) {
    case AICA_MM_MEMALIGN:
        if (arg0 > 4)
            resp = (unsigned int)aligned_alloc(arg0, arg1);
        else
            resp = (unsigned int)malloc(arg1);
        break;
    case AICA_MM_REALLOC:
        resp = (unsigned int)realloc((void *)arg0, arg1);
        break;
    case AICA_MM_AVAILABLE:
        resp = mem_available();
        break;
    case AICA_MM_FREE:
        free((void *)arg0);
        return;
    default:
        /* Unknown command */
        return;
    }

    aica_send_response_code(header, resp);
}

static void process_reserve(struct aica_header *header)
{
    unsigned char ch = aica_reserve_channel();

    aica_send_response_code(header, ch);
}

void aica_process_command(struct aica_header *header, struct aica_cmd *cmd) {
    /* Figure out what type of packet it is */
    switch(cmd->cmd) {
        case AICA_CMD_RESERVE:
            if (cmd->misc[0] == (unsigned int)-1)
                process_reserve(header);
            else
                aica_unreserve_channel(cmd->misc[0]);
            break;

        case AICA_CMD_CHAN:
            process_chn(header, cmd, (aica_channel_t *)cmd->cmd_data);
            break;

        case AICA_CMD_MM:
            process_mm(header, cmd->cmd_id, cmd->misc[0], cmd->misc[1]);
            break;

        case AICA_CMD_S3MPLAY:
            if (!cmd->misc[2])
                s3m_play((struct s3m_header *)cmd->misc[0], cmd->misc[1]);
            else
                s3m_stop((struct s3m_header *)cmd->misc[0]);
            break;

        case AICA_CMD_INFO:
            task_fill_info(&tasks_info.info);
            aica_send_response_code(header, (unsigned int)&tasks_info);
            break;

        default:
            /* error */
            break;
    }
}

int main(int argc, char **argv) {
    unsigned int i;

    printf("AICA firmware initialized.\n");

    for (;;) {
        /* Update channel position counters */
        for(i = 0; i < 64; i++)
            aica_header.channels[i].pos = aica_get_pos(i);

        /* Little delay to prevent memory lock */
        task_sleep(ms_to_ticks(10));
    }
}
