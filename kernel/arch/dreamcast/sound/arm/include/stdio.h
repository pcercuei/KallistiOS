#ifndef __AICAOS_STDIO_H
#define __AICAOS_STDIO_H

int aica_printf(const char *restrict fmt, ...);
#define printf(...) aica_printf(__VA_ARGS__)

#endif /* __AICAOS_STDIO_H */
