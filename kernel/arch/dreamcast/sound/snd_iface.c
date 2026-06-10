/* KallistiOS ##version##

   snd_iface.c
   Copyright (C) 2000-2002 Megan Potter
   Copyright (C) 2024 Ruslan Rostovtsev

   SH-4 support routines for accessing the AICA via the standard KOS driver
*/

#include <kos/mutex.h>
#include <dc/aica.h>
#include <dc/spu.h>
#include <dc/sound/sound.h>

#define AICA_RAM_START      0x030000

/* Are we initted? */
static int initted = 0;

static mutex_t getpos_lock = MUTEX_INITIALIZER;

/* Initialize driver; note that this replaces the AICA program so that
   if you had anything else going on, it's gone now! */
int snd_init(void) {
    /* Finish loading the stream driver */
    if(!initted) {
        spu_disable();

         /* Initialize the RAM allocator */
        snd_mem_init(AICA_RAM_START);
    }

    initted = 1;

    return 0;
}

/* Shut everything down and free mem */
void snd_shutdown(void) {
    if(initted) {
        snd_mem_shutdown();
        initted = 0;
    }
}

uint16_t snd_get_pos(unsigned int ch) {
    mutex_lock_scoped(&getpos_lock);

    return aica_get_pos_unlocked(ch);
}

bool snd_is_playing(unsigned int ch) {
    return aica_is_started(ch);
}
