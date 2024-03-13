/* KallistiOS ##version##

   snd_mem.c
   Copyright (C) 2002 Megan Potter
   Copyright (C) 2023 Ruslan Rostovtsev

 */

#include <dc/sound/sound.h>

/* Deprecated. Empty stub. */
int snd_mem_init(uint32 start, unsigned int size) {
    return 0;
}

/* Deprecated. Empty stub. */
void snd_mem_shutdown(void) {
}

/* Allocate a chunk of SPU RAM; we will return an offset into SPU RAM. */
uint32 snd_mem_malloc(size_t size) {
    aica_cmd_t cmd = {
        .size = sizeof(cmd) / 4,
        .cmd = AICA_CMD_MM,
        .cmd_id = AICA_MM_MEMALIGN,
        .misc[0] = 32, /* align to 32 bytes for DMA */
        .misc[1] = (unsigned int)size,
    };

    return snd_sh4_to_aica_with_response(&cmd);
}

/* Free a chunk of SPU RAM; pointer is expected to be an offset into
   SPU RAM. */
void snd_mem_free(uint32 addr) {
    aica_cmd_t cmd = {
        .size = sizeof(cmd) / 4,
        .cmd = AICA_CMD_MM,
        .cmd_id = AICA_MM_FREE,
        .misc[0] = addr,
    };

    snd_sh4_to_aica(&cmd, cmd.size);
}

uint32 snd_mem_available(void) {
    aica_cmd_t cmd = {
        .size = sizeof(cmd) / 4,
        .cmd = AICA_CMD_MM,
        .cmd_id = AICA_MM_AVAILABLE,
    };

    return snd_sh4_to_aica_with_response(&cmd);
}
