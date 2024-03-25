/* KallistiOS ##version##

   main.c
   (c)2000-2002 Megan Potter

   Generic sound driver with streaming capabilities

   This slightly more complicated version allows for sound effect channels,
   and full sampling rate, panning, and volume control for each.

*/

#include <aicaos/aica.h>
#include <cmd_iface.h>
#include <stddef.h>

/****************** Timer *******************************************/

#define timer (*((volatile uint32 *)AICA_MEM_CLOCK))

void timer_wait(uint32 jiffies) {
    uint32 fin = timer + jiffies;

    while(timer <= fin)
        ;
}

/****************** Main Program ************************************/

/* Our SH-4 interface (statically placed memory structures) */
volatile aica_queue_t   *q_cmd = (volatile aica_queue_t *)AICA_MEM_CMD_QUEUE;
volatile aica_queue_t   *q_resp = (volatile aica_queue_t *)AICA_MEM_RESP_QUEUE;

/* Process a CHAN command */
void process_chn(struct aica_header *header, aica_cmd_t *pkt, aica_channel_t *chndat) {
    uint32 cmd_id = pkt->cmd_id;
    struct aica_channel *chn = &header->channels[cmd_id];
    unsigned int flags;

    switch(chndat->cmd & AICA_CH_CMD_MASK) {
        case AICA_CH_CMD_NONE:
            break;
        case AICA_CH_CMD_START:

            if(chndat->cmd & AICA_CH_START_SYNC) {
                aica_sync_play(cmd_id);
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

/* Process one packet of queue data */
uint32 process_one(struct aica_header *header, uint32 tail) {
    uint32      pktdata[AICA_CMD_MAX_SIZE], *pdptr, size, i;
    volatile uint32 * src;
    aica_cmd_t  * pkt;

    src = (volatile uint32 *)(q_cmd->data + tail);
    pkt = (aica_cmd_t *)pktdata;
    pdptr = pktdata;

    /* Get the size field */
    size = *src;

    if(size > AICA_CMD_MAX_SIZE)
        size = AICA_CMD_MAX_SIZE;

    /* Copy out the packet data */
    for(i = 0; i < size; i++) {
        *pdptr++ = *src++;

        if((uint32)src >= (q_cmd->data + q_cmd->size))
            src = (volatile uint32 *)q_cmd->data;
    }

    /* Figure out what type of packet it is */
    switch(pkt->cmd) {
        case AICA_CMD_NONE:
            break;
        case AICA_CMD_PING:
            /* Not implemented yet */
            break;
        case AICA_CMD_CHAN:
            process_chn(header, pkt, (aica_channel_t *)pkt->cmd_data);
            break;
        case AICA_CMD_SYNC_CLOCK:
            /* Reset our timer clock to zero */
            timer = 0;
            break;
        default:
            /* error */
            break;
    }

    return size;
}

/* Look for an available request in the command queue; if one is there
   then process it and move the tail pointer. */
void process_cmd_queue(struct aica_header *header) {
    uint32      head, tail, tsloc, ts;

    /* Grab these values up front in case SH-4 changes head */
    head = q_cmd->head;
    tail = q_cmd->tail;

    /* Do we have anything to process? */
    while(head != tail) {
        /* Look at the next packet. If our clock isn't there yet, then
           we won't process anything yet either. */
        tsloc = tail + offsetof(aica_cmd_t, timestamp);

        if(tsloc >= q_cmd->size)
            tsloc -= q_cmd->size;

        ts = *((volatile uint32*)(q_cmd->data + tsloc));

        if(ts > 0 && ts >= timer)
            return;

        /* Process it */
        ts = process_one(header, tail);

        /* Ok, skip over the packet */
        tail += ts * 4;

        if(tail >= q_cmd->size)
            tail -= q_cmd->size;

        q_cmd->tail = tail;
    }
}

int main(int argc, char **argv) {
    int i;

    /* Setup our queues */
    q_cmd->head = q_cmd->tail = 0;
    q_cmd->data = AICA_MEM_CMD_QUEUE + sizeof(aica_queue_t);
    q_cmd->size = AICA_MEM_RESP_QUEUE - q_cmd->data;
    q_cmd->process_ok = 1;
    q_cmd->valid = 1;

    q_resp->head = q_resp->tail = 0;
    q_resp->data = AICA_MEM_RESP_QUEUE + sizeof(aica_queue_t);
    q_resp->size = AICA_MEM_CHANNELS - q_resp->data;
    q_resp->process_ok = 1;
    q_resp->valid = 1;

    /* Wait for a command */
    for(; ;) {
        /* Update channel position counters */
        for(i = 0; i < 64; i++)
            aica_header.channels[i].pos = aica_get_pos(i);

        /* Check for a command */
        if(q_cmd->process_ok)
            process_cmd_queue(&aica_header);

        /* Little delay to prevent memory lock */
        timer_wait(10);
    }
}
