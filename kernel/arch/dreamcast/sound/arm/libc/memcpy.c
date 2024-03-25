#include <stddef.h>
#include <string.h>

void * memcpy(void *dest, const void *src, size_t count) {
    unsigned char *tmp = (unsigned char *) dest;
    unsigned char *s = (unsigned char *) src;

    while(count--)
        *tmp++ = *s++;

    return dest;
}
