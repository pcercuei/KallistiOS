#include <stddef.h>
#include <string.h>

size_t strlen(const char *s)
{
    const char *ptr = s;

    while (*ptr)
        ptr++;

    return ptr - s;
}
