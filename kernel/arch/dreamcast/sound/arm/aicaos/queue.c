#include <aicaos/aica.h>
#include <aicaos/irq.h>
#include <aicaos/queue.h>
#include <aicaos/task.h>

#include <cmd_iface.h>
#include <registers.h>
#include <stddef.h>

static struct task cmd_task;
static unsigned int cmd_task_stack[0x400];

static _Bool notified;

extern volatile unsigned int timer;

__attribute__((weak)) void
aica_process_command(struct aica_header *header, struct aica_cmd *cmd)
{
}

/* Process one packet of queue data */
static uint32_t process_one(struct aica_header *header, uint32_t tail) {
    volatile struct aica_queue *q_cmd = header->cmd_queue;
    uint32_t pktdata[AICA_CMD_MAX_SIZE], *pdptr, size, i;
    volatile uint32_t *src;
    aica_cmd_t  * pkt;

    src = (volatile uint32_t *)(q_cmd->data + tail);
    pkt = (aica_cmd_t *)pktdata;
    pdptr = pktdata;

    /* Get the size field */
    size = *src;

    if(size > AICA_CMD_MAX_SIZE)
        size = AICA_CMD_MAX_SIZE;

    /* Copy out the packet data */
    for(i = 0; i < size; i++) {
        *pdptr++ = *src++;

        if((uint32_t)src >= (q_cmd->data + q_cmd->size))
            src = (volatile uint32_t *)q_cmd->data;
    }

    aica_process_command(header, pkt);

    return size;
}

/* Look for an available request in the command queue; if one is there
   then process it and move the tail pointer. */
static void process_cmd_queue(struct aica_header *header) {
    volatile struct aica_queue *q_cmd = header->cmd_queue;
    uint32_t head, tail, tsloc, ts;

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

        ts = *((volatile uint32_t *)(q_cmd->data + tsloc));

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

    task_init(&cmd_task, "queue", aica_read_queue, params, TASK_PRIO_HIGH,
              cmd_task_stack, sizeof(cmd_task_stack));
}

void aica_notify_queue(void)
{
    notified = 1;
    task_wake(&aica_header, 1);
}
