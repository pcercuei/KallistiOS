/* KallistiOS ##version##

   queue.c
   Copyright (C) 2026 Paul Cercueil

   SH4<->ARM queue mechanism and RPC interface
*/

#include <kos/dbglog.h>
#include <kos/errno.h>
#include <kos/genwait.h>
#include <kos/irq.h>
#include <kos/rpc.h>

#include <aica/irq.h>
#include <aica/queue.h>
#include <aica/registers.h>

#include <stdbool.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <string.h>

alignas(32)
static rpc_cmd_t in_buffer[64];

alignas(32)
static rpc_cmd_t out_buffer[64];

static rpc_queue_t aica_out_queue = {
    .addr = out_buffer,
    .size = __array_size(out_buffer),
};

static rpc_queue_t aica_in_queue = {
    .addr = in_buffer,
    .size = __array_size(in_buffer),
};

static aica_header_t aica_header = {
    .arm_queue = &aica_in_queue,
    .sh4_queue = &aica_out_queue,
};

static void aica_rpc_copy(void *dst, const void *src, size_t len) {
    memcpy(dst, src, len);
}

rpc_t aica_rpc = {
    .outbound = &aica_out_queue,
    .inbound = &aica_in_queue,
    .rpc_read = aica_rpc_copy,
    .rpc_write = aica_rpc_copy,
    .rpc_notify = irq_interrupt_sh4,
};

static void aica_notify_queue(irq_t code, irq_context_t *context, void *d) {
    (void)code;
    (void)context;

    /* Ack the SH4 interrupt */
    SPU_REG32(REG_SPU_INT_RESET) = SPU_INT_ENABLE_SH4;

    rpc_process_inbound(d);
}

void queue_init(void) {
    rpc_init(&aica_rpc);

    irq_set_handler(EXC_SH4, aica_notify_queue, &aica_rpc);

    /* Tell the SH4 where our header is */
    *(void **)AICA_HEADER_ADDR = &aica_header;
}

void queue_shutdown(void) {
    irq_set_handler(EXC_SH4, NULL, NULL);
    rpc_shutdown();
}
