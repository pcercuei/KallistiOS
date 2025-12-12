/* KallistiOS ##version##

   init.c
   Copyright (C) 2026 Paul Cercueil

   AICA init code
*/

#include <kos/banner.h>
#include <kos/dbgio.h>
#include <kos/dbglog.h>
#include <kos/fs.h>
#include <kos/fs_dev.h>
#include <kos/fs_null.h>
#include <kos/fs_pty.h>
#include <kos/fs_ramdisk.h>
#include <kos/fs_romdisk.h>
#include <kos/init.h>
#include <kos/irq.h>
#include <kos/linker.h>
#include <kos/mm.h>
#include <kos/nmmgr.h>
#include <kos/rtc.h>
#include <kos/rpc.h>
#include <kos/thread.h>

#include <arch/timer.h>
#include <aica/queue.h>
#include <aica/vfs.h>

#include <stdlib.h>
#include <string.h>

void (*__kos_init_early_fn)(void) __attribute__((weak,section(".data"))) = NULL;

extern int main(int argc, char **argv);
extern void _init(void);
extern void _fini(void);

extern dbgio_handler_t dbgio_aica;

void fs_romdisk_mount_builtin(void) {
    fs_romdisk_mount("/rd", __kos_romdisk, false);
}

KOS_INIT_FLAG_WEAK(fs_dev_init, true);
KOS_INIT_FLAG_WEAK(fs_dev_shutdown, true);
KOS_INIT_FLAG_WEAK(fs_shutdown, true);
KOS_INIT_FLAG_WEAK(fs_null_init, true);
KOS_INIT_FLAG_WEAK(fs_null_shutdown, true);
KOS_INIT_FLAG_WEAK(fs_pty_init, true);
KOS_INIT_FLAG_WEAK(fs_pty_shutdown, true);
KOS_INIT_FLAG_WEAK(fs_ramdisk_init, true);
KOS_INIT_FLAG_WEAK(fs_ramdisk_shutdown, true);
KOS_INIT_FLAG_WEAK(fs_romdisk_init, true);
KOS_INIT_FLAG_WEAK(fs_romdisk_shutdown, true);
KOS_INIT_FLAG_WEAK(fs_romdisk_mount_builtin, false);

__noreturn void arch_main(void) {
    int rv;

    /* Handle optional callback provided by KOS_INIT_EARLY() */
    if(__kos_init_early_fn)
        __kos_init_early_fn();

    /* Clear out the BSS area */
    memset(_bss_start, 0, (uintptr_t)end - (uintptr_t)_bss_start);

    mm_init();
    irq_init();

    timer_init();
    nmmgr_init();
    fs_init();

    thd_init();
    queue_init();
    irq_enable();

    vfs_sh4_init();                       /* /sh4 */

    if(__kos_init_flags & INIT_QUIET) {
        dbgio_disable();
    }
    else {
        dbgio_add_handler(&dbgio_aica);
        dbgio_init();
    }

    KOS_INIT_FLAG_CALL(fs_pty_init);      /* /pty */

    dbglog(DBG_INFO, "%s", kos_get_banner());

    KOS_INIT_FLAG_CALL(fs_dev_init);      /* /dev */
    KOS_INIT_FLAG_CALL(fs_null_init);     /* /dev/null */

    KOS_INIT_FLAG_CALL(fs_ramdisk_init);  /* /ram */
    KOS_INIT_FLAG_CALL(fs_romdisk_init);  /* /rd */
    KOS_INIT_FLAG_CALL(fs_romdisk_mount_builtin);

    rtc_init();

    /* Run ctors */
    _init();

    /* Call the user's main function */
    rv = main(0, NULL);

    /* Call kernel exit */
    exit(rv);
}

__noreturn
static void arch_real_exit(void) {
    irq_disable();
    timer_shutdown();
    thd_shutdown();
    irq_shutdown();

    while(1);
}

__noreturn
void arch_abort(void) {
    dbglog(DBG_CRITICAL, "arch: aborting the system\n");

    arch_real_exit();
}

__noreturn
void arch_shutdown(void) {
    /* Run dtors */
    _fini();

    dbglog(DBG_CRITICAL, "arch: shutting down kernel\n");

    KOS_INIT_FLAG_CALL(fs_ramdisk_shutdown);
    KOS_INIT_FLAG_CALL(fs_romdisk_shutdown);
    KOS_INIT_FLAG_CALL(fs_null_shutdown);
    KOS_INIT_FLAG_CALL(fs_dev_shutdown);
    fs_shutdown();
    KOS_INIT_FLAG_CALL(fs_pty_shutdown);
    vfs_sh4_shutdown();

    arch_real_exit();
}

__noreturn
void arch_exit_handler(int ret_code) {
    dbglog(DBG_INFO, "\narch: exit return code %d\n", ret_code);

    arch_shutdown();
}

/* Generic kernel exit point */
void arch_exit(void) {
    /* arch_exit always returns EXIT_SUCCESS (0)
       if return codes are desired then a call to
       newlib's exit() should be used in its place */
    exit(EXIT_SUCCESS);
}

/* Called by GCC 8.5.0 when using atomic_store() for some reason */
void __sync_synchronize(void) {
}
