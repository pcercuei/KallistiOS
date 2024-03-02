/* KallistiOS ##version##

   aica_cmd_iface.h
   (c)2000-2002 Megan Potter

   Definitions for the SH-4/AICA interface. This file is meant to be
   included from both the ARM and SH-4 sides of the fence.
*/

#ifndef __ARM_AICA_IRQ_H
#define __ARM_AICA_IRQ_H

#include <registers.h>

#define SPU_CPSR_F_BIT SPU_BIT(6)
#define SPU_CPSR_I_BIT SPU_BIT(7)

typedef unsigned int irq_ctx_t;

irq_ctx_t irq_disable(void);
void irq_restore(irq_ctx_t ctx);

static inline void __irq_scoped_cleanup(irq_ctx_t *state) {
    irq_restore(*state);
}

#define ___irq_disable_scoped(l) \
    irq_ctx_t __scoped_irq_##l __attribute__((cleanup(__irq_scoped_cleanup))) = irq_disable()

#define __irq_disable_scoped(l) ___irq_disable_scoped(l)

#define irq_disable_scoped() __irq_disable_scoped(__LINE__)

_Bool irq_enabled(void);

void aica_interrupt_init(void);

/* Interrupt the SH4 */
void aica_interrupt(void);

#endif /* __ARM_AICA_IRQ_H */
