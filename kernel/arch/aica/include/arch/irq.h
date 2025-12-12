/* KallistiOS ##version##

   arch/aica/include/arch/irq.h
   Copyright (C) 2025 Paul Cercueil

*/

/** \file    arch/irq.h
    \brief   Interrupt and exception handling.
    \ingroup irqs

    This file contains various definitions and declarations related to handling
    interrupts on the AICA.

    \author Paul Cercueil
*/

/* Keep this include above the macro guards */
#include <kos/irq.h>

#ifndef __ARCH_IRQ_H
#define __ARCH_IRQ_H

#include <kos/cdefs.h>
__BEGIN_DECLS

#include <kos/regfield.h>
#include <aica/registers.h>

#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

#define SPU_CPSR_F_BIT BIT(6)
#define SPU_CPSR_I_BIT BIT(7)

struct __attribute__((aligned(32))) irq_context {
    unsigned int r0_r7[8];
    unsigned int pc;
    unsigned int r8_r14[7];
    unsigned int cpsr;
};

enum irq_exception
#ifdef __cplusplus
: unsigned int
#endif
{
    EXC_TIMER           = 2,
    EXC_SH4             = 4,
    EXC_BUS             = 5,
};

/** \name Register Accessors
    \brief Convenience macros for accessing context registers
    @{
*/
/** Fetch the program counter from a struct irq_context.
    \param  c               The context to read from.
    \return                 The program counter value.
*/
#define CONTEXT_PC(c)   ((c).pc)

/** Fetch the frame pointer from a struct irq_context.
    \param  c               The context to read from.
    \return                 The frame pointer value.
*/
#define CONTEXT_FP(c)   ((c).r8_r14[3])

/** Fetch the stack pointer from a struct irq_context.
    \param  c               The context to read from.
    \return                 The stack pointer value.
*/
#define CONTEXT_SP(c)   ((c).r8_r14[5])

/** Fetch the return value from a struct irq_context.
    \param  c               The context to read from.
    \return                 The return value.
*/
#define CONTEXT_RET(c)  ((c).r0_r7[0])
/** @} */

extern int inside_int;

static inline int arch_irq_inside_int(void) {
    return inside_int;
}

static inline void arch_irq_restore(irq_mask_t old) {
    __asm__ volatile("msr CPSR_all,%0" : : "r"(old));
}

static inline irq_mask_t arch_irq_disable(void) {
    irq_mask_t cpsr;

    __asm__ volatile("mrs %0,CPSR_all" : "=r"(cpsr) :);

    arch_irq_restore(cpsr | SPU_CPSR_F_BIT | SPU_CPSR_I_BIT);

    return cpsr;
}

static inline void arch_irq_enable(void) {
    irq_mask_t cpsr;

    __asm__ volatile("mrs %0,CPSR_all" : "=r"(cpsr) :);

    arch_irq_restore(cpsr & ~(SPU_CPSR_F_BIT | SPU_CPSR_I_BIT));
}

void arch_irq_create_context(struct irq_context *context,
                             uintptr_t stack_pointer,
                             uintptr_t routine,
                             const uintptr_t *args);

int arch_irq_set_handler(enum irq_exception code, irq_hdl_t hnd, void *data);

irq_cb_t arch_irq_get_handler(enum irq_exception code);

int arch_irq_set_global_handler(irq_hdl_t hnd, void *data);

irq_cb_t arch_irq_get_global_handler(void);

void arch_irq_set_context(struct irq_context *cxt);

struct irq_context *arch_irq_get_context(void);

/* Include <kos/irq.h> for compatibility with code that includes <arch/irq.h> instead. */
#include <kos/irq.h>

__END_DECLS

#endif  /* __ARCH_IRQ_H */
