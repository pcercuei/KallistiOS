/* KallistiOS ##version##

   snd_iface.c
   (c)2000-2002 Megan Potter

   SH-4 support routines for accessing the AICA via the standard KOS driver
*/

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <kos/sem.h>
#include <arch/timer.h>
#include <dc/aram.h>
#include <dc/asic.h>
#include <dc/g2bus.h>
#include <dc/spu.h>
#include <dc/sound/cmd_iface.h>
#include <dc/sound/registers.h>
#include <dc/sound/sound.h>


/* Forward declarations */
static int snd_from_aica(struct aica_header *header, void *packetout);

/* Are we initted? */
static int initted = 0;

/* This will come from a separately linked object file */
extern uint8_t snd_stream_drv[];
extern uint8_t snd_stream_drv_end[];

/* Thread semaphore */
static semaphore_t sem_qram;

struct aica_header aica_header;

static void snd_callback(uint32_t source, void *data)
{
    struct aica_header *header = data;
    uint32_t pkt[AICA_CMD_MAX_SIZE];
    aica_cmd_t *pktcmd = (aica_cmd_t *)pkt;
    uint32_t str_addr, size;
    char buf[256];
    int ret;

    (void)source;

    for (;;) {
        ret = snd_from_aica(header, pkt);
        if (ret < 1)
            return;

        switch (pktcmd->cmd) {
        default:
            dbglog(DBG_DEBUG, "Unhandled command from ARM: %lu\n",
                   pktcmd->cmd);
            break;
        }
    }
}

static void snd_ack_arm_irq(uint16_t source)
{
    (void)source;

    g2_write_32(REG_SPU_SH4_INT_RESET, SPU_INT_ENABLE_SH4);
}

/* Initialize driver; note that this replaces the AICA program so that
   if you had anything else going on, it's gone now! */
int snd_init(void) {
    aram_addr_t header_addr;
    int amt, ret, flags;
    unsigned int i;

    /* Finish loading the stream driver */
    if(!initted) {
        spu_disable();
        asic_evt_disable(ASIC_EVT_SPU_IRQ, ASIC_IRQ9);

        /* Even with the asic_evt_disable() above, the ARM is still able to send
           interrupts; so we need to disable them completely. */
        flags = irq_disable();

        /* Cancel any pending interrupt from the ARM */
        snd_ack_arm_irq(0);

        /* Register a handler for the interrupt */
        ret = asic_evt_request_threaded_handler(ASIC_EVT_SPU_IRQ, snd_callback,
                                                &aica_header, snd_ack_arm_irq,
                                                NULL);
        if (ret < 0) {
            dbglog(DBG_ERROR, "snd_init(): unable to request threaded interrupt\n");
            spu_disable();
            return ret;
        }

        amt = (snd_stream_drv_end - snd_stream_drv + 3) & ~3;

        dbglog(DBG_DEBUG, "snd_init(): loading %d bytes into SPU RAM\n", amt);
        spu_memload_sq(0, snd_stream_drv, amt);

        /* Clear header address so that we can detect it when it's set */
        g2_fifo_wait();
        aram_write_32(AICA_HEADER_ADDR, 0);

        /* Enable the AICA and give it a few ms to start up */
        spu_enable();

        for (i = 0; i < 100; i++) {
            timer_spin_sleep(10);

            /* Get the address of the firmware header */
            header_addr = (aram_addr_t)aram_read_32(AICA_HEADER_ADDR);
            if (header_addr != 0)
                break;
        }

        if (header_addr == 0) {
            dbglog(DBG_ERROR, "snd_init(): ARM firmware did not wake up\n");
            spu_disable();
            return -1;
        }

        dbglog(DBG_DEBUG, "snd_init(): Firmware header is at ARAM address 0x%lx\n",
               header_addr);

        /* Read the header */
        aram_read(&aica_header, header_addr, sizeof(aica_header));

        dbglog(DBG_DEBUG, "snd_init(): Samples buffer is at ARAM address 0x%lx, size 0x%x\n",
               (aram_addr_t)aica_header.buffer, aica_header.buffer_size);

        /* Initialize the RAM allocator for the sample buffer */
        snd_mem_init((uint32)aica_header.buffer, aica_header.buffer_size);

        /* Setup semaphores */
        sem_init(&sem_qram, 1);

        asic_evt_enable(ASIC_EVT_SPU_IRQ, ASIC_IRQ9);
        irq_restore(flags);
    }

    initted = 1;

    return 0;
}

/* Shut everything down and free mem */
void snd_shutdown(void) {
    if(initted) {
        spu_disable();

        asic_evt_remove_handler(ASIC_EVT_SPU_IRQ);
        asic_evt_disable(ASIC_EVT_SPU_IRQ, ASIC_IRQ9);

        sem_destroy(&sem_qram);
        snd_mem_shutdown();
        initted = 0;
    }
}

/* Submit a request to the SH4->AICA queue; size is in uint32's */
int snd_sh4_to_aica(void *packet, uint32_t size) {
    uint32_t  *pkt32, cnt;
    aram_addr_t start, top;
    aica_queue_t cmd_queue;

    assert_msg(size < 256, "SH4->AICA packets may not be >256 uint32's long");

    sem_wait(&sem_qram);

    /* Read the command queue's structure */
    aram_read(&cmd_queue, (aram_addr_t)aica_header.cmd_queue, sizeof(cmd_queue));

    assert_msg(cmd_queue.valid, "Queue is not yet valid");

    top = (aram_addr_t)(cmd_queue.data + cmd_queue.size);
    start = (aram_addr_t)(cmd_queue.data + cmd_queue.head);
    pkt32 = (uint32 *)packet;
    cnt = 0;

    while(size > 0) {
        /* Fifo wait if necessary */
        if(!(cnt % 8))
            g2_fifo_wait();

        cnt++;

        /* Write the next dword */
        aram_write_32(start, *pkt32++);

        /* Move our counters */
        start += 4;

        if(start >= top)
            start = cmd_queue.data;

        size--;
    }

    /* Finally, write a new head value to signify that we've added
       a packet for it to process */
    aram_write_32((aram_addr_t)aica_header.cmd_queue + offsetof(aica_queue_t, head),
                  start - cmd_queue.data);

    /* We could wait until head == tail here for processing, but there's
       not really much point; it'll just slow things down. */

    sem_signal(&sem_qram);

    return 0;
}

/* Start processing requests in the queue */
void snd_sh4_to_aica_start(void) {
    aram_write_32((aram_addr_t)aica_header.cmd_queue + offsetof(aica_queue_t, process_ok), 1);
}

/* Stop processing requests in the queue */
void snd_sh4_to_aica_stop(void) {
    aram_write_32((aram_addr_t)aica_header.cmd_queue + offsetof(aica_queue_t, process_ok), 0);
}

static int snd_from_aica(struct aica_header *header, void *packetout) {
    uint32 size, cnt, *pkt32;
    aram_addr_t top, start, stop;
    aica_queue_t resp_queue;

    sem_wait(&sem_qram);

    /* Read the response queue's structure */
    aram_read(&resp_queue, (aram_addr_t)header->resp_queue, sizeof(resp_queue));

    assert_msg(resp_queue.valid, "Queue is not yet valid");

    top = resp_queue.data + resp_queue.size;
    start = resp_queue.data + resp_queue.tail;
    stop = resp_queue.data + resp_queue.head;
    pkt32 = (uint32 *)packetout;
    cnt = 0;

    /* Is there anything? */
    if(start == stop) {
        sem_signal(&sem_qram);
        return 0;
    }

    /* Check for packet size overflow */
    size = aram_read_32(start + offsetof(aica_cmd_t, size));

    if(cnt >= AICA_CMD_MAX_SIZE) {
        sem_signal(&sem_qram);
        dbglog(DBG_ERROR, "snd_aica_to_sh4(): packet larger than %d dwords\n", AICA_CMD_MAX_SIZE);
        return -1;
    }

    /* Find stop point for this packet */
    stop = start + size * 4;

    if(stop > top)
        stop -= top;

    while(start != stop) {
        /* Fifo wait if necessary */
        if(!(cnt % 8))
            g2_fifo_wait();

        cnt++;

        /* Read the next dword */
        *pkt32++ = aram_read_32(start);

        /* Move our counters */
        start += 4;

        if(start >= top)
            start = resp_queue.data;

        cnt++;
    }

    /* Finally, write a new tail value to signify that we've removed a packet */
    aram_write_32((aram_addr_t)header->resp_queue + offsetof(aica_queue_t, tail),
                  start - resp_queue.data);

    sem_signal(&sem_qram);

    return 1;
}

/* Transfer one packet of data from the AICA->SH4 queue. Expects to
   find AICA_CMD_MAX_SIZE dwords of space available. Returns -1
   if failure, 0 for no packets available, 1 otherwise. Failure
   might mean a permanent failure since the queue is probably out of sync. */
int snd_aica_to_sh4(void *packetout) {
    return snd_from_aica(&aica_header, packetout);
}

/* Old function to poll for responses from the AICA. Not used anymore,
 * as the AICA will raise interrupts when it has something to send. */
void snd_poll_resp(void) {
}
