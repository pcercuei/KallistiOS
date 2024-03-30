#ifndef __AICAOS_STDIO_H
#define __AICAOS_STDIO_H

typedef int FILE;

enum {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
};

#define EOF ((char)-1)

int aica_printf(const char *restrict fmt, ...);
#define printf(...) aica_printf(__VA_ARGS__)

#endif /* __AICAOS_STDIO_H */
