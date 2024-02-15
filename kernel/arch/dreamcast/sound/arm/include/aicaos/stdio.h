#ifndef __AICAOS_STDIO_H
#define __AICAOS_STDIO_H

__attribute__((format(printf,1,2)))
int aica_printf(const char *restrict fmt, ...);

#endif /* __AICAOS_STDIO_H */
