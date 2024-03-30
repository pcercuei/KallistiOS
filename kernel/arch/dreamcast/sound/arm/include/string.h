#ifndef __KALLISTIOS_ARM_STRING_H
#define __KALLISTIOS_ARM_STRING_H

#include <stddef.h>

void * memcpy(void *dest, const void *src, size_t count);
void * memset(void *dest, int c, size_t count);
int memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);

#endif /* __KALLISTIOS_ARM_STRING_H */
