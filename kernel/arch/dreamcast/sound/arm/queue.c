
#include "aica_cmd_iface.h"
#include "aica_registers.h"
#include "aica.h"
#include "irq.h"
#include "lock.h"
#include "queue.h"
#include "task.h"

#include <stdarg.h>
#include <stddef.h>

static struct task cmd_task;
static unsigned int cmd_task_stack[0x400];

static _Bool notified;

extern volatile unsigned int timer;

static struct mutex queue_lock = MUTEX_INITIALIZER;

static void aica_add_cmd(struct aica_header *header,
                         const struct aica_cmd *cmd) {
    volatile struct aica_queue *q_resp = header->resp_queue;
    uint32 top, start, stop, *pkt32 = (uint32 *)cmd;

    mutex_lock(&queue_lock);

    top = q_resp->data + q_resp->size;
    start = q_resp->data + q_resp->head;
    stop = start + cmd->size * 4;

    if (stop > top)
        stop -= top;

    while (start != stop) {
        *(uint32 *)start = *pkt32++;

        start += 4;
        if (start >= top)
            start = q_resp->data;
    }

    /* Finally, write the new head value to signify that we've added a packet */
    q_resp->head = start - q_resp->data;

    /* Ping the SH4 */
    aica_interrupt();

    mutex_unlock(&queue_lock);
}

static void aica_vprintf(const char *fmt, va_list ap) {
    struct aica_cmd cmd = {
        .size = sizeof(struct aica_cmd) / 4,
        .cmd = AICA_RESP_DBGPRINT,
        .misc = {
            [0] = (unsigned int)fmt,
            [1] = va_arg(ap, unsigned int),
            [2] = va_arg(ap, unsigned int),
            [3] = va_arg(ap, unsigned int),
        },
    };

    aica_add_cmd(&aica_header, &cmd);
}

void aica_printf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    aica_vprintf(fmt, ap);
    va_end(ap);
}

/* Process a CHAN command */
static void
process_chn(struct aica_header *header, aica_cmd_t *pkt, aica_channel_t *chndat) {
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

static void send_response_code(struct aica_header *header, unsigned int code)
{
    struct aica_cmd cmd = {
        .size = sizeof(struct aica_cmd) / 4,
        .cmd = AICA_RESP,
        .misc[0] = code,
    };

    aica_add_cmd(header, &cmd);
}

static void process_reserve(struct aica_header *header)
{
    unsigned char ch = aica_reserve_channel();

    send_response_code(header, ch);
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
        case AICA_CMD_RESERVE:
            if (pkt->misc[0] == (unsigned int)-1)
                process_reserve(header);
            else
                aica_unreserve_channel(pkt->misc[0]);
            break;

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
