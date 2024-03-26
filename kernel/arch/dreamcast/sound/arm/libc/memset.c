#include <stddef.h>
#include <string.h>

void * memset(void *dest, int c, size_t count) {
    unsigned char *tmp = (unsigned char *) dest;

    while(count--)
        *tmp++ = (unsigned char)c;

    return dest;
}
