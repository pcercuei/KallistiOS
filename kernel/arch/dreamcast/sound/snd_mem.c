/* KallistiOS ##version##

   snd_mem.c
   Copyright (C) 2002 Megan Potter
   Copyright (C) 2023, 2025 Ruslan Rostovtsev

 */

#include <dc/sound/sound.h>
#include <kos/rpc.h>
#include <aica/queue.h>

/* Allocate a chunk of SPU RAM; we will return an offset into SPU RAM. */
uint32_t snd_mem_malloc(size_t size) {
    uint32_t align = 8;
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_MEMALIGN,
        .params = {
            [0] = align,
            [1] = size,
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}

/* Free a chunk of SPU RAM; pointer is expected to be an offset into
   SPU RAM. */
void snd_mem_free(uint32_t addr) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_FREE,
        .flags = RPC_CMD_FLAG_ASYNC,
        .params = {
            [0] = addr,
        },
    };

    rpc_execute(&aica_rpc, &cmd);
}

uint32_t snd_mem_available(void) {
    return 0; /* TODO? */
}
