#ifndef __AICA_MM_H
#define __AICA_MM_H

void aica_mm_init(void);

void * aica_malloc(unsigned int size);
void * aica_memalign(unsigned int align, unsigned int size);
void * aica_realloc(void *ptr, unsigned int size);
void aica_free(void *ptr);

unsigned int aica_available(void);

#endif /* __AICA_MM_H */
