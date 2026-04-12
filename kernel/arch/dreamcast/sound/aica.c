/* KallistiOS ##version##

   aica.c
   Copyright (C) 2026 Paul Cercueil

   AICA API implementation on the SH-4 side
*/

#include <aica/aica.h>
#include <aica/queue.h>
#include <dc/aram.h>
#include <kos/rpc.h>

#include <stdint.h>

#define BITLL(x) (1ull << (x))

void aica_init(void) {
}

void aica_shutdown(void) {
}

void aica_configure(uint8_t chn, const aica_chn_data_t *data) {
    aram_write((aram_addr_t)&aica_header.channels[chn], data, sizeof(*data));
}

void aica_update(uint8_t chn) {
    aica_update_channels(BITLL(chn));
}

void aica_start(uint8_t chn) {
    aica_start_channels(BITLL(chn));
}

void aica_stop(uint8_t chn) {
    aica_stop_channels(BITLL(chn));
}

void aica_update_channels(uint64_t mask) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_UPDATE,
        .flags = RPC_CMD_FLAG_ASYNC,
        .params[0] = (uint32_t)mask,
        .params[1] = (uint32_t)(mask >> 32),
    };

    rpc_execute(&aica_rpc, &cmd);
}

void aica_start_channels(uint64_t mask) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_START,
        .flags = RPC_CMD_FLAG_ASYNC,
        .params[0] = (uint32_t)mask,
        .params[1] = (uint32_t)(mask >> 32),
    };

    rpc_execute(&aica_rpc, &cmd);
}

void aica_stop_channels(uint64_t mask) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_STOP,
        .flags = RPC_CMD_FLAG_ASYNC,
        .params[0] = (uint32_t)mask,
        .params[1] = (uint32_t)(mask >> 32),
    };

    rpc_execute(&aica_rpc, &cmd);
}

int8_t aica_reserve_channel(void) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_CHN_REQUEST,
    };

    return rpc_execute(&aica_rpc, &cmd);
}

void aica_unreserve_channel(uint8_t ch) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_CHN_REQUEST,
        .flags = RPC_CMD_FLAG_ASYNC,
        .params[0] = ch,
    };

    rpc_execute(&aica_rpc, &cmd);
}
