static inline void *memcpy(void *dest, const void *src, unsigned int n)
{
    char *dst = dest;

    while (n--)
        *(char *)dst++ = *(const char *)src++;

    return dest;
}
