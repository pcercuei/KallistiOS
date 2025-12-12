/* KallistiOS ##version##

   arch/aica/include/arch.h
   Copyright (C) 2001 Megan Potter
   Copyright (C) 2013, 2020 Lawrence Sebald
   Copyright (C) 2026 Paul Cercueil

*/

/** \file    arch/arch.h
    \brief   AICA architecture specific options.
    \ingroup arch

    This file has various architecture specific options defined in it. Also, any
    functions that start with arch_ are in here.

    \author Megan Potter
*/

#ifndef __ARCH_ARCH_H
#define __ARCH_ARCH_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <kos/thread.h>

#include <stdbool.h>

#include <arch/types.h>

/** \defgroup arch  Architecture
    \brief          AICA Architecture-Specific Options and high-level API
    \ingroup        system
    @{
*/

/** \brief  Top of memory available */
#define _arch_mem_top   ((uint32_t) 0x00200000)

/** \brief  Start and End address for .text portion of program. */
extern char _executable_start;
extern char _etext;

#define PAGESIZE        4096            /**< \brief Page size (for MMU) */
#if 1
#define PAGESIZE_BITS   12              /**< \brief Bits for page size */
#define PAGEMASK        (PAGESIZE - 1)  /**< \brief Mask for page offset */

/** \brief  Page count "variable".

    The number of pages is static, so we can optimize this quite a bit. */
#define page_count      ((_arch_mem_top - page_phys_base) / PAGESIZE)

/** \brief  Base address of available physical pages. */
#define page_phys_base  0x00000000
#endif

#ifndef THD_SCHED_HZ
/** \brief Scheduler interrupt frequency

    Timer interrupt frequency for the KOS thread scheduler.

    \note
    This value is what KOS uses initially upon startup, but it can be
    reconfigured at run-time.

    \sa thd_get_hz(), thd_set_hz()
*/
#define THD_SCHED_HZ    25
#endif

/** Legacy symbol for scheduler frequency.
 *  \deprecated
 *  \sa THD_SCHED_HZ
 */
static const
unsigned HZ __depr("Please use the new THD_SCHED_HZ macro.") = THD_SCHED_HZ;

/** \brief  Global symbol prefix in ELF files. */
#define ELF_SYM_PREFIX      "_"

/** \brief  Length of global symbol prefix in ELF files. */
#define ELF_SYM_PREFIX_LEN  1

/** \brief  Standard name for this arch. */
#define ARCH_NAME           "AICA"

/** \brief  ELF class for this architecture. */
#define ARCH_ELFCLASS       ELFCLASS32

/** \brief  ELF data encoding for this architecture. */
#define ARCH_ELFDATA        ELFDATA2LSB

/** \brief  ELF machine type code for this architecture. */
#define ARCH_CODE           EM_ARM

/** \brief  Panic function.

    This function will cause a kernel panic, printing the specified message.

    \param  str             The error message to print.
    \note                   This function will never return!
*/
void arch_panic(const char *str) __noreturn;

/** \brief  Kernel C-level entry point.
    \note                   This function will never return!
*/
void arch_main(void) __noreturn;

/** @} */

/** \defgroup arch_retpaths Exit Paths
    \brief                  Potential exit paths from the kernel on
                            arch_exit()
    \ingroup                arch
    @{
*/
#define ARCH_EXIT_RETURN    1   /**< \brief Return to loader */
#define ARCH_EXIT_MENU      2   /**< \brief Return to system menu */
#define ARCH_EXIT_REBOOT    3   /**< \brief Reboot the machine */
/** @} */

/** \brief   Set the exit path.
    \ingroup arch

    The default, if you don't call this, is ARCH_EXIT_RETURN.

    \param  path            What arch_exit() should do.
    \see    arch_retpaths
*/
void arch_set_exit_path(int path);

/** \brief   Generic kernel "exit" point.
    \ingroup arch
    \note                   This function will never return!
*/
void arch_exit(void) __noreturn;

/** \brief   Kernel "return" point.
    \ingroup arch
    \note                   This function will never return!
*/
void arch_return(int ret_code) __noreturn;

/** \brief   Kernel "abort" point.
    \ingroup arch
    \note                   This function will never return!
*/
void arch_abort(void) __noreturn;

/** \brief   Kernel "reboot" call.
    \ingroup arch
    \note                   This function will never return!
*/
void arch_reboot(void) __noreturn;

/** \brief   Kernel "exit to menu" call.
    \ingroup arch
    \note                   This function will never return!
*/
void arch_menu(void) __noreturn;

/** \brief   Determine how much memory is installed in current machine.
    \ingroup arch

    \return The total size of system memory in bytes.
*/
#define HW_MEMSIZE 0x00200000

/* Bring in the init flags for compatibility with old code that expects them
   here. */
#include <kos/init.h>

/* AICA-specific arch init things */
/** \brief   Jump back to the bootloader.
    \ingroup arch

    You generally shouldn't use this function, but rather use arch_exit() or
    exit() instead.

    \note                   This function will never return!
*/
void arch_real_exit(int ret_code) __noreturn;

/** \brief   Initialize bare-bones hardware systems.
    \ingroup arch

    This will be done automatically for you on start by the default arch_main(),
    so you shouldn't have to deal with this yourself.

    \retval 0               On success (no error conditions defined).
*/
int hardware_sys_init(void);

/** \brief   Initialize some peripheral systems.
    \ingroup arch

    This will be done automatically for you on start by the default arch_main(),
    so you shouldn't have to deal with this yourself.

    \retval 0               On success (no error conditions defined).
*/
int hardware_periph_init(void);

/** \brief   Shut down hardware that was initted.
    \ingroup arch

    This function will shut down anything initted with hardware_sys_init() and
    hardware_periph_init(). This will be done for you automatically by the
    various exit points, so you shouldn't have to do this yourself.
*/
void hardware_shutdown(void);

/* These three aught to be in their own header file at some point, but for now,
   they'll stay here. */

/** \brief   Retrieve the banner printed at program initialization.
    \ingroup attribution

    This function retrieves the banner string that is printed at initialization
    time by the kernel. This contains the version of KOS in use and basic
    information about the environment in which it was compiled.

    \return                 A pointer to the banner string.
*/
const char *kos_get_banner(void);

/** \brief   Retrieve the license information for the compiled copy of KOS.
    \ingroup attribution

    This function retrieves a string containing the license terms that the
    version of KOS in use is distributed under. This can be used to easily add
    information to your program to be displayed at runtime.

    \return                 A pointer to the license terms.
*/
const char *kos_get_license(void);

/** \brief   Retrieve a list of authors and the dates of their contributions.
    \ingroup attribution

    This function retrieves the copyright information for the version of KOS in
    use. This function can be used to add such information to the credits of
    programs using KOS to give the appropriate credit to those that have worked
    on KOS.

    \remark
    Remember, you do need to give credit where credit is due, and this is an
    easy way to do so. ;-)

    \return                 A pointer to the authors' copyright information.
*/
const char *kos_get_authors(void);

static inline void arch_sleep(void) {
    /* Nothing to do */
    thd_pass();
}

/** \brief   DC specific "function" to get the return address from the current
             function.
    \ingroup arch

    \return                 The return address of the current function.
*/
static __always_inline uintptr_t arch_get_ret_addr(void) {
#if 0
    uintptr_t pr;

    __asm__ __volatile__("sts pr,%0\n" : "=r"(pr));

    return pr;
#else
    return 0;
#endif
}

/* Please note that all of the following frame pointer macros are ONLY
   valid if you have compiled your code WITHOUT -fomit-frame-pointer. These
   are mainly useful for getting a stack trace from an error. */

/** \brief   DC specific "function" to get the frame pointer from the current
             function.
    \ingroup arch

    \return                 The frame pointer from the current function.
    \note                   This only works if you don't disable frame pointers.
*/
static __always_inline uintptr_t arch_get_fptr(void) {
    register uintptr_t fp __asm__("r14");

    return fp;
}

/** \brief   Pass in a frame pointer value to get the return address for the
             given frame.
    \ingroup arch

    \param  fptr            The frame pointer to look at.
    \return                 The return address of the pointer.
*/
static inline uintptr_t arch_fptr_ret_addr(uintptr_t fptr) {
    return *(uintptr_t *)fptr;
}

/** \brief   Pass in a frame pointer value to get the previous frame pointer for
             the given frame.
    \ingroup arch

    \param  fptr            The frame pointer to look at.
    \return                 The previous frame pointer.
*/
static inline uintptr_t arch_fptr_next(uintptr_t fptr) {
    return arch_fptr_ret_addr(fptr + 4);
}

/** \brief   Returns true if the passed address is likely to be valid. Doesn't
             have to be exact, just a sort of general idea.
    \ingroup arch

    \return                 Whether the address is valid or not for normal
                            memory access.
*/
static inline bool arch_valid_address(uintptr_t ptr) {
    return ptr < _arch_mem_top;
}

/** \brief   Returns true if the passed address is in the text section of your
             program.
    \ingroup arch

    \return                 Whether the address is valid or not for text
                            memory access.
*/
static inline bool arch_valid_text_address(uintptr_t ptr) {
    return ptr >= (uintptr_t)&_executable_start && ptr < (uintptr_t)&_etext;
}

__END_DECLS

#endif  /* __ARCH_ARCH_H */
