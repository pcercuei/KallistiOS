#ifndef __AICA_LIBC_STDLIB_H
#define __AICA_LIBC_STDLIB_H

#include <stddef.h>

void * malloc(size_t size);
void * calloc(size_t nb, size_t size);
void * realloc(void *ptr, unsigned int size);
void free(void *ptr);

void * aligned_alloc(size_t align, size_t size);
int posix_memalign(void **addr, size_t align, size_t size);

unsigned int mem_available(void);

#endif /* __AICA_LIBC_STDLIB_H */
