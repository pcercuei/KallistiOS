/* KallistiOS ##version##

   aica_cmd_iface.h
   (c)2000-2002 Megan Potter

   Definitions for the SH-4/AICA interface. This file is meant to be
   included from both the ARM and SH-4 sides of the fence.
*/

#ifndef __ARM_AICA_IRQ_H
#define __ARM_AICA_IRQ_H

#include "aica_registers.h"

#define SPU_CPSR_F_BIT SPU_BIT(6)
#define SPU_CPSR_I_BIT SPU_BIT(7)

typedef unsigned int irq_ctx_t;

irq_ctx_t irq_disable(void);
void irq_restore(irq_ctx_t ctx);

_Bool irq_enabled(void);

void aica_interrupt_init(void);

/* Interrupt the SH4 */
void aica_interrupt(void);

#endif /* __ARM_AICA_IRQ_H */
