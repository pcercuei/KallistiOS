/* KallistiOS ##version##

   puts.c
   Copyright (C) 2026 Paul Cercueil

   puts() wrapper to bypass complex string format processing
*/

#include <aica/queue.h>
#include <kos/rpc.h>

int __wrap_puts(const char *str) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_PUTS,
        .flags = RPC_CMD_FLAG_ASYNC,
        .params = {
            [0] = (uint32_t)str,
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}
