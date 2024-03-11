/* KallistiOS ##version##

   mm.c
   Copyright (C ) 2024 Paul Cercueil

   Memory management routines
*/

#include "lock.h"
#include "tlsf/tlsf.h"

#include <stddef.h>

static tlsf_t tlsf;
static struct mutex mm_lock = MUTEX_INITIALIZER;

extern unsigned int __heap_start, __heap_end;

void aica_mm_init(void)
{
    unsigned int size = (unsigned int)&__heap_end - (unsigned int)&__heap_start;

    tlsf = tlsf_create_with_pool(&__heap_start, size);
}

void * aica_malloc(unsigned int size)
{
    void *addr;

    mutex_lock(&mm_lock);
    addr = tlsf_malloc(tlsf, size);
    mutex_unlock(&mm_lock);

    return addr;
}

void * aica_memalign(unsigned int align, unsigned int size)
{
    void *addr;

    mutex_lock(&mm_lock);
    addr = tlsf_memalign(tlsf, align, size);
    mutex_unlock(&mm_lock);

    return addr;
}

void * aica_realloc(void *ptr, unsigned int size)
{
    void *addr;

    mutex_lock(&mm_lock);
    addr = tlsf_realloc(tlsf, ptr, size);
    mutex_unlock(&mm_lock);

    return addr;
}

void aica_free(void *ptr)
{
    mutex_lock(&mm_lock);
    tlsf_free(tlsf, ptr);
    mutex_unlock(&mm_lock);
}

static void aica_sum_free(void *ptr, size_t size, int used, void *user)
{
    unsigned int *available = user;

    if (!used)
        *available += (unsigned int)size;
}

unsigned int aica_available(void)
{
    unsigned int available = 0;
    pool_t pool = tlsf_get_pool(tlsf);

    mutex_lock(&mm_lock);
    tlsf_walk_pool(pool, aica_sum_free, &available);
    mutex_unlock(&mm_lock);

    return available;
}
