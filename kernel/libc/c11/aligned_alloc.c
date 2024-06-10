/* KallistiOS ##version##

   aligned_alloc.c
   Copyright (C) 2015 Lawrence Sebald
*/

#include <stdlib.h>

/* Declare memalign, since it may not be available in strict standard-compliant
   mode. */
extern void *memalign(size_t alignment, size_t size);

void *aligned_alloc(size_t alignment, size_t size) {
    if(size % alignment)
        return NULL;

    return memalign(alignment, size);
}
