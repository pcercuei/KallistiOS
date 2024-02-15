#include <aicaos/queue.h>
#include <aica_comm.h>
#include <stdarg.h>

static void aica_vprintf(const char *fmt, va_list ap) {
    struct aica_cmd cmd = {
        .size = sizeof(struct aica_cmd) / 4,
        .cmd = AICA_RESP_DBGPRINT,
        .misc = {
            [0] = (unsigned int)fmt,
            [1] = va_arg(ap, unsigned int),
            [2] = va_arg(ap, unsigned int),
            [3] = va_arg(ap, unsigned int),
        },
    };

    aica_add_cmd(&cmd);
}

int aica_printf(const char *restrict fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    aica_vprintf(fmt, ap);
    va_end(ap);

    return 0;
}
