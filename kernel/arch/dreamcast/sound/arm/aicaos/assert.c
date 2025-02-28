#include <aicaos/stdio.h>
#include <stddef.h>

void __assert(const char *file, int line, const char *expr, const char *msg, const char *func)
{
	aica_printf("Assertion failure\n");
}

void __assert_func(const char *file, int line, const char *func, const char *expr) {
	__assert(file, line, expr, NULL, func);
}
