/* KallistiOS ##version##

   aram.c
   Copyright (C) 2024 Paul Cercueil <paul@crapouillou.net>
*/

#include <stdint.h>
#include <dc/g2bus.h>
#include <dc/aram.h>

static void aram_copy(char *dst, const char *src, size_t size)
{
    uint32_t cnt = 0;
    g2_ctx_t ctx;

    ctx = g2_lock();

    if (!(((uintptr_t)dst | (uintptr_t)src) & 0x3)) {
        for (; size > 4; size -= 4) {
            /* Fifo wait if necessary */
            if (!(cnt % 8))
                g2_fifo_wait();

            *(uint32_t *)dst = *(const uint32_t *)src;
            dst += 4;
            src += 4;
            cnt++;
        }
    }

    for (; size; size--) {
        /* Fifo wait if necessary */
        if (!(cnt % 8))
            g2_fifo_wait();

        *dst++ = *src++;
        cnt++;
    }

    g2_unlock(ctx);
}

void aram_read(void *dst, aram_addr_t addr, size_t size)
{
    const char *src = (const char *)aram_addr_to_host(addr);

    aram_copy((char *)dst, src, size);
}

void aram_write(aram_addr_t addr, const void *src, size_t size)
{
    char *dst = (char *)aram_addr_to_host(addr);

    aram_copy(dst, (const char *)src, size);
}
