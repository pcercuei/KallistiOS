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

#include <stdalign.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
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

static aica_chn_data_t aica_channels[64];

aica_header_t aica_header = {
    .arm_queue = &aica_in_queue,
    .sh4_queue = &aica_out_queue,
    .channels = aica_channels,
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

static int aica_memalign(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    return (int)aligned_alloc(cmd->params[0], cmd->params[1]);
}

static int aica_realloc(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    return (int)realloc((void *)cmd->params[0], cmd->params[1]);
}

static int aica_free(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    free((void *)cmd->params[0]);
    return 0;
}

static int aica_chn_request(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;
    (void)cmd;

    return aica_reserve_channel();
}

static int aica_chn_release(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    aica_unreserve_channel(cmd->params[0]);
    return 0;
}

static inline uint64_t cmd_get_mask(const rpc_cmd_t *cmd) {
    return ((uint64_t)cmd->params[1] << 32) | cmd->params[0];
}

static int aica_chn_update(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    aica_update_channels(cmd_get_mask(cmd));
    return 0;
}

static int aica_chn_start(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    aica_start_channels(cmd_get_mask(cmd));
    return 0;
}

static int aica_chn_stop(const rpc_cmd_t *cmd, void *cb_data) {
    (void)cb_data;

    aica_stop_channels(cmd_get_mask(cmd));
    return 0;
}

void queue_init(void) {
    rpc_init(&aica_rpc);

    rpc_register(AICA_CMD_MEMALIGN, aica_memalign, NULL);
    rpc_register(AICA_CMD_REALLOC, aica_realloc, NULL);
    rpc_register(AICA_CMD_FREE, aica_free, NULL);
    rpc_register(AICA_CMD_CHN_REQUEST, aica_chn_request, NULL);
    rpc_register(AICA_CMD_CHN_RELEASE, aica_chn_release, NULL);
    rpc_register(AICA_CMD_UPDATE, aica_chn_update, NULL);
    rpc_register(AICA_CMD_START, aica_chn_start, NULL);
    rpc_register(AICA_CMD_STOP, aica_chn_stop, NULL);

    irq_set_handler(EXC_SH4, aica_notify_queue, &aica_rpc);

    /* Tell the SH4 where our header is */
    *(void **)AICA_HEADER_ADDR = &aica_header;
}

void queue_shutdown(void) {
    irq_set_handler(EXC_SH4, NULL, NULL);
    rpc_shutdown();
}
