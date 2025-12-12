/* KallistiOS ##version##

   arch/aica/include/arch/stack.h
   (c)2002 Megan Potter

*/

/** \file    arch/stack.h
    \brief   Stack tracing.
    \ingroup debugging_stacktrace

    The functions in this file deal with doing stack traces. These functions
    will do a stack trace, as specified, printing it out to stdout (usually a
    dcload terminal). These functions only work if frame pointers have been
    enabled at compile time (-DFRAME_POINTERS and no -fomit-frame-pointer flag).

    \author Megan Potter
*/

#ifndef __ARCH_STACK_H
#define __ARCH_STACK_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <stdint.h>

#ifndef THD_STACK_ALIGNMENT
/** \brief  Required alignment for stack. */
#define THD_STACK_ALIGNMENT 8
#endif

#ifndef THD_STACK_SIZE
/** \brief  Default thread stack size. */
#define THD_STACK_SIZE  32768
#endif

#ifndef THD_KERNEL_STACK_SIZE
/** \brief Main/kernel thread's stack size. */
#define THD_KERNEL_STACK_SIZE (64 * 1024)
#endif

struct kthread;

void arch_stk_setup(struct kthread *nt);

/** \defgroup debugging_stacktrace  Stack Traces
    \brief                          API for managing stack backtracing
    \ingroup                        debugging

    @{
*/

/** \brief  Do a stack trace from the current function.

    This function does a stack trace from the current function, printing the
    results to stdout. This is used, for instance, when an assertion fails in
    assert().

    \param  n               The number of frames to leave off. Each frame is a
                            jump to subroutine or branch to subroutine. assert()
                            leaves off 2 frames, for reference.
*/
void arch_stk_trace(int n);

/** \brief  Do a stack trace from the current function.

    This function does a stack trace from the the specified frame pointer,
    printing the results to stdout. This could be used for doing something like
    stack tracing a main thread from inside an IRQ handler.

    \param  fp              The frame pointer to start from.
    \param  n               The number of frames to leave off.
*/
void arch_stk_trace_at(uint32_t fp, size_t n);

/** @} */

__END_DECLS

#endif  /* __ARCH_EXEC_H */

