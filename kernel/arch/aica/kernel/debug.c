/* KallistiOS ##version##

   debug.c
   Copyright (C) 2026 Paul Cercueil

   AICA dbgio interface
*/

#include <kos/dbgio.h>

#include <errno.h>
#include <stdio.h>

static file_t pty;

static int dbgio_aica_write_buffer(const uint8_t *data, int len, int xlat) {
    (void)xlat;

    return fs_write(pty, data, len);
}

static int dbgio_aica_always_detected(void) {
    return 1;
}

static int dbgio_aica_init(void) {
    pty = fs_open("/sh4/pty/sl00", O_RDWR);
    if(pty == FILEHND_INVALID)
        return -1;

    return 0;
}

static int dbgio_aica_shutdown(void) {
    fs_close(pty);

    return 0;
}

static int dbgio_aica_flush(void) {
    return 0;
}

static int dbgio_aica_read_buffer(uint8_t *data, int len) {
    return fs_read(pty, data, len);
}

dbgio_handler_t dbgio_aica = {
    .name = "aica",
    .detected = dbgio_aica_always_detected,
    .write_buffer = dbgio_aica_write_buffer,
    .init = dbgio_aica_init,
    .shutdown = dbgio_aica_shutdown,
    .flush = dbgio_aica_flush,
    .read_buffer = dbgio_aica_read_buffer,
};
