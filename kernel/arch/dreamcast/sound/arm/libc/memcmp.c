#include <stddef.h>
#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = s1;
    const unsigned char *b = s2;

    while (n--) {
        if (*a != *b)
            return *a < *b ? -1 : 1;

        if (!a)
            break;
    }

    return 0;
}
