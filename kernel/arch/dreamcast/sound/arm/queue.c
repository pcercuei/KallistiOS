
#include "aica_cmd_iface.h"
#include "aica_registers.h"
#include "aica.h"
#include "irq.h"
#include "queue.h"
#include "task.h"

#include <stddef.h>

static struct task cmd_task;
static unsigned int cmd_task_stack[0x400];

static _Bool notified;

extern volatile unsigned int timer;

/* Process a CHAN command */
static void
process_chn(struct aica_header *header, aica_cmd_t *pkt, aica_channel_t *chndat) {
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
static uint32 process_one(struct aica_header *header, uint32 tail) {
    volatile struct aica_queue *q_cmd = header->cmd_queue;
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
        case AICA_CMD_CHAN:
            process_chn(header, pkt, (aica_channel_t *)pkt->cmd_data);
            break;
        default:
            /* error */
            break;
    }

    return size;
}

/* Look for an available request in the command queue; if one is there
   then process it and move the tail pointer. */
static void process_cmd_queue(struct aica_header *header) {
    volatile struct aica_queue *q_cmd = header->cmd_queue;
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

static void aica_read_queue(struct aica_header *header)
{
    irq_ctx_t cxt;

    for (;;) {
        cxt = irq_disable();

        if (!notified)
            task_wait(header);

        notified = 0;

        irq_restore(cxt);

        /* Process the command queue */
        if (header->cmd_queue->process_ok)
            process_cmd_queue(header);
    }
}

void aica_init_queue(struct aica_header *header)
{
    unsigned int params[4] = { (unsigned int)header, };

    task_init(&cmd_task, aica_read_queue, params, TASK_PRIO_HIGH,
              cmd_task_stack, sizeof(cmd_task_stack));
}

void aica_notify_queue(void)
{
    notified = 1;
    task_wake(&aica_header, 1);
}
