/* KallistiOS ##version##

   snd_iface.c
   Copyright (C) 2000-2002 Megan Potter
   Copyright (C) 2024 Ruslan Rostovtsev

   SH-4 support routines for accessing the AICA via the standard KOS driver
*/

#include <stdalign.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <kos/dbglog.h>
#include <kos/errno.h>
#include <kos/thread.h>
#include <kos/mutex.h>
#include <kos/timer.h>
#include <dc/aram.h>
#include <dc/asic.h>
#include <dc/g2bus.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>
#include <dc/sound/queue.h>
#include <aica/queue.h>
#include <aica/registers.h>

#include "arm/aica_cmd_iface.h"

/* Include the default firmware blob */
#include "snd_stream_drv.c"

/* Are we initted? */
static int initted = 0;

/* The queue processing mutex for snd_sh4_to_aica_start and snd_sh4_to_aica_stop.
   There are some cases like stereo stream control + stereo sfx control
   at the same time in separate threads. */
static mutex_t queue_proc_mutex = MUTEX_INITIALIZER;

static void aica_rpc_read(void *dst, const void *rpc_src, size_t len) {
    aram_read(dst, (aram_addr_t)rpc_src, len);
}

static void aica_rpc_write(void *rpc_dst, const void *src, size_t len) {
    aram_write((aram_addr_t)rpc_dst, src, len);
}

static void aica_rpc_notify(void) {
    g2_write_32(REG_SPU_INT_SEND, SPU_INT_ENABLE_SH4);
}

rpc_t aica_rpc = {
    .rpc_read = aica_rpc_read,
    .rpc_write = aica_rpc_write,
    .rpc_notify = aica_rpc_notify,
};

static int process_puts(const rpc_cmd_t *cmd, void *d) {
    alignas(4) char buf[1024];
    (void)d;

    puts(aram_read_string(cmd->params[0], buf, sizeof(buf)));

    return 0;
}

static int process_openfile(const rpc_cmd_t *cmd, void *d) {
    alignas(4) char buf[1024];
    char *fn;

    (void)d;
    fn = aram_read_string((aram_addr_t)cmd->params[0], buf, sizeof(buf));

    return fs_open(fn, cmd->params[1]);
}

static int process_writefile(const rpc_cmd_t *cmd, void *d) {
    alignas(4) char buf[1024];
    int fd = cmd->params[0];
    aram_addr_t src = cmd->params[1];
    size_t cnt = cmd->params[2];

    (void)d;
    aram_read(buf, src & ~0x3, cnt + (src & 0x3));

    return (int)fs_write(fd, &buf[src & 0x3], cnt);
}

static int process_readfile(const rpc_cmd_t *cmd, void *d) {
    alignas(4) char buf[1024];
    int fd = cmd->params[0];
    aram_addr_t dst = cmd->params[1];
    size_t cnt = cmd->params[2];
    ssize_t ret;

    (void)d;
    assert_msg(!(dst & 0x3), "Dest ARAM address must be 4-byte aligned");

    if(cnt > sizeof(buf))
        cnt = sizeof(buf);

    ret = fs_read(fd, buf, cnt);
    if(ret > 0)
        aram_write(dst, buf, ret);

    return (int)ret;
}

static int process_closefile(const rpc_cmd_t *cmd, void *d) {
    (void)d;
    return fs_close(cmd->params[0]);
}

static int process_seekfile(const rpc_cmd_t *cmd, void *d) {
    (void)d;
    return fs_seek(cmd->params[0], cmd->params[1], cmd->params[2]);
}

static int process_tellfile(const rpc_cmd_t *cmd, void *d) {
    (void)d;
    return fs_tell(cmd->params[0]);
}

static int process_totalfile(const rpc_cmd_t *cmd, void *d) {
    (void)d;
    return fs_total(cmd->params[0]);
}

static int process_readdir(const rpc_cmd_t *cmd, void *d) {
    aram_addr_t dst = cmd->params[1];
    const dirent_t *dirent;

    (void)d;
    assert_msg(!(dst & 0x3), "Dest ARAM address must be 4-byte aligned");

    dirent = fs_readdir(cmd->params[0]);

    if(!dirent)
        return 0;

    aram_write(dst, dirent, sizeof(*dirent));

    return dst;
}

static int snd_read_header(void *d) {
    uint32_t hdr;
    (void)d;

    /* Get the address of the firmware header */
    hdr = aram_read_32(AICA_HEADER_ADDR);
    if(!hdr)
        return 0;

    /* Read twice to be sure */
    if(hdr != aram_read_32(AICA_HEADER_ADDR))
        return 0;

    return hdr;
}

static void snd_ack_arm_irq(void) {
    g2_write_32(REG_SPU_SH4_INT_RESET, SPU_INT_ENABLE_SH4);
}

static void snd_callback(uint32_t source, void *data) {
    (void)source;

    snd_ack_arm_irq();
    rpc_process_inbound(data);
}

/* Initialize driver; note that this replaces the AICA program so that
   if you had anything else going on, it's gone now! */
int snd_init(void) {
    struct aica_header aica_header;
    aram_addr_t header_addr;
    size_t amt;

    /* Finish loading the stream driver */
    if(!initted) {
        spu_disable();

        spu_memset_sq(0, 0, AICA_RAM_START);
        amt = __align_up(snd_stream_drv_size, 4);

        asic_evt_disable(ASIC_EVT_SPU_IRQ, ASIC_IRQ9);

        /* Even with the asic_evt_disable() above, the ARM is still able to send
           interrupts; so we need to disable them completely. */
        irq_disable_scoped();

        /* Cancel any pending interrupt from the ARM */
        snd_ack_arm_irq();

        dbglog(DBG_DEBUG, "snd_init(): loading %zu bytes into SPU RAM\n", amt);
        spu_memload_sq(0, (void *)snd_stream_drv_data, amt);

        /* Clear header address so that we can detect it when it's set */
        aram_write_32(AICA_HEADER_ADDR, 0);

        /* Enable the AICA and give it a few ms to start up */
        spu_enable();

        header_addr = thd_poll(snd_read_header, NULL, 200);
        if(!header_addr) {
            dbglog(DBG_ERROR, "snd_init(): ARM firmware did not wake up\n");
            spu_disable();
            return -1;
        }

        dbglog(DBG_DEBUG, "snd_init(): Firmware header is at ARAM address 0x%lx\n",
               header_addr);

        /* Read the header */
        aram_read(&aica_header, header_addr, sizeof(aica_header));

        aica_rpc.inbound = aica_header.sh4_queue;
        aica_rpc.outbound = aica_header.arm_queue;

        rpc_init(&aica_rpc);

        rpc_register(AICA_CMD_PUTS, process_puts, NULL);
        rpc_register(AICA_CMD_OPENFILE, process_openfile, NULL);
        rpc_register(AICA_CMD_WRITEFILE, process_writefile, NULL);
        rpc_register(AICA_CMD_READFILE, process_readfile, NULL);
        rpc_register(AICA_CMD_CLOSEFILE, process_closefile, NULL);
        rpc_register(AICA_CMD_SEEKFILE, process_seekfile, NULL);
        rpc_register(AICA_CMD_TELLFILE, process_tellfile, NULL);
        rpc_register(AICA_CMD_TOTALFILE, process_totalfile, NULL);
        rpc_register(AICA_CMD_READDIR, process_readdir, NULL);

        asic_evt_set_handler(ASIC_EVT_SPU_IRQ, snd_callback, &aica_rpc);

        /* Enable IRQs from the ARM */
        asic_evt_enable(ASIC_EVT_SPU_IRQ, ASIC_IRQ9);

        /* Initialize the RAM allocator */
        snd_mem_init(AICA_RAM_START);
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

        snd_mem_shutdown();
        initted = 0;
    }
}

/* Submit a request to the SH4->AICA queue; size is in uint32's */
int snd_sh4_to_aica(void *packet, uint32_t size) {
    uint32_t qa, bot, start, top, *pkt32, cnt;
    assert_msg(size < AICA_CMD_MAX_SIZE, "SH4->AICA packets may not be >256 uint32's long");

    g2_lock_scoped();

    /* Set these up for reference */
    qa = SPU_RAM_UNCACHED_BASE + AICA_MEM_CMD_QUEUE;
    assert_msg(g2_read_32_raw(qa + offsetof(aica_queue_t, valid)), "Queue is not yet valid");

    bot = SPU_RAM_UNCACHED_BASE + g2_read_32_raw(qa + offsetof(aica_queue_t, data));
    top = bot + g2_read_32_raw(qa + offsetof(aica_queue_t, size));
    start = bot + g2_read_32_raw(qa + offsetof(aica_queue_t, head));
    pkt32 = (uint32_t *)packet;
    cnt = 0;

    while(size-- > 0) {
        /* Fifo wait if necessary */
        if((cnt++ & 7) == 0)
            g2_fifo_wait();

        /* Write the next dword */
        g2_write_32_raw(start, *pkt32++);

        /* Move our counters */
        start += 4;

        if(start >= top)
            start = bot;
    }

    /* Finally, write a new head value to signify that we've added
       a packet for it to process */
    if((cnt & 7) == 0)
        g2_fifo_wait();

    g2_write_32_raw(qa + offsetof(aica_queue_t, head), start - bot);

    /* We could wait until head == tail here for processing, but there's
       not really much point; it'll just slow things down. */
    return 0;
}

/* Start processing requests in the queue */
void snd_sh4_to_aica_start(void) {
    g2_write_32(SPU_RAM_UNCACHED_BASE + AICA_MEM_CMD_QUEUE + offsetof(aica_queue_t, process_ok), 1);
    mutex_unlock(&queue_proc_mutex);
}

/* Stop processing requests in the queue */
void snd_sh4_to_aica_stop(void) {
    mutex_lock(&queue_proc_mutex);
    g2_write_32(SPU_RAM_UNCACHED_BASE + AICA_MEM_CMD_QUEUE + offsetof(aica_queue_t, process_ok), 0);
}

/* Transfer one packet of data from the AICA->SH4 queue. Expects to
   find AICA_CMD_MAX_SIZE dwords of space available. Returns -1
   if failure, 0 for no packets available, 1 otherwise. Failure
   might mean a permanent failure since the queue is probably out of sync. */
int snd_aica_to_sh4(void *packetout) {
    uint32  bot, start, stop, top, size, cnt, *pkt32;

    g2_lock_scoped();

    /* Set these up for reference */
    bot = SPU_RAM_UNCACHED_BASE + AICA_MEM_RESP_QUEUE;
    assert_msg(g2_read_32_raw(bot + offsetof(aica_queue_t, valid)), "Queue is not yet valid");

    top = SPU_RAM_UNCACHED_BASE + AICA_MEM_RESP_QUEUE + g2_read_32_raw(bot + offsetof(aica_queue_t, size));
    start = SPU_RAM_UNCACHED_BASE + AICA_MEM_RESP_QUEUE + g2_read_32_raw(bot + offsetof(aica_queue_t, tail));
    stop = SPU_RAM_UNCACHED_BASE + AICA_MEM_RESP_QUEUE + g2_read_32_raw(bot + offsetof(aica_queue_t, head));
    cnt = 0;
    pkt32 = (uint32_t *)packetout;

    /* Is there anything? */
    if(start == stop) {
        return 0;
    }

    /* Check for packet size overflow */
    size = g2_read_32_raw(start + offsetof(aica_cmd_t, size));

    if(size >= AICA_CMD_MAX_SIZE) {
        dbglog(DBG_ERROR, "snd_aica_to_sh4(): packet larger than %d dwords\n", AICA_CMD_MAX_SIZE);
        return -1;
    }

    /* Find stop point for this packet */
    stop = start + size * 4;

    if(stop > top)
        stop -= top - (SPU_RAM_UNCACHED_BASE + AICA_MEM_RESP_QUEUE);

    while(start != stop) {
        /* Fifo wait if necessary */
        if((cnt++ & 7) == 0)
            g2_fifo_wait();

        /* Read the next dword */
        *pkt32++ = g2_read_32_raw(start);

        /* Move our counters */
        start += 4;

        if(start >= top)
            start = bot;
    }

    /* Finally, write a new tail value to signify that we've removed a packet */
    if((cnt & 7) == 0)
        g2_fifo_wait();

    g2_write_32_raw(bot + offsetof(aica_queue_t, tail), start - (SPU_RAM_UNCACHED_BASE + AICA_MEM_RESP_QUEUE));

    return 1;
}

/* Poll for responses from the AICA. We assume here that we're not
   running in an interrupt handler (thread perhaps, of whoever
   is using us). */
void snd_poll_resp(void) {
    int rv;
    uint32_t pkt[AICA_CMD_MAX_SIZE];
    aica_cmd_t *pktcmd;

    pktcmd = (aica_cmd_t *)pkt;

    while((rv = snd_aica_to_sh4(pkt)) > 0) {
        dbglog(DBG_DEBUG, "snd_poll_resp(): Received packet id %08lx, ts %08lx from AICA\n",
               pktcmd->cmd, pktcmd->timestamp);
    }

    if(rv < 0)
        dbglog(DBG_ERROR, "snd_poll_resp(): snd_aica_to_sh4 failed, giving up\n");
}

uint16_t snd_get_pos(unsigned int ch) {
    return g2_read_32(SPU_RAM_UNCACHED_BASE + AICA_CHANNEL(ch) + offsetof(aica_channel_t, pos)) & 0xffff;
}

bool snd_is_playing(unsigned int ch) {
    return g2_read_32(MEM_AREA_P2_BASE + 0x00700000 + 0x80 * ch) & AICA_CHANNEL_KEYONB;
}
