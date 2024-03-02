/* KallistiOS ##version##

   irq.c
   Copyright (C) 2024 Paul Cercueil

   IRQ handling routines for the ARM
*/

#include "irq.h"

void irq_restore(irq_ctx_t ctx)
{
    __asm__ volatile("msr CPSR_c,%0" : : "r"(ctx));
}

irq_ctx_t irq_disable(void)
{
    register unsigned int cpsr;

    __asm__ volatile("mrs %0,CPSR" : "=r"(cpsr) :);

    irq_restore(cpsr | SPU_CPSR_F_BIT | SPU_CPSR_I_BIT);

    return cpsr;
}

_Bool irq_enabled(void)
{
    register unsigned int cpsr;

    __asm__ volatile("mrs %0,CPSR" : "=r"(cpsr) :);

    return !(cpsr & (SPU_CPSR_F_BIT | SPU_CPSR_I_BIT));
}

void aica_interrupt_init(void)
{
    /* Program the FIQ codes */
    SPU_REG32(REG_SPU_FIQ_BIT_2) =
        ((SPU_INT_SH4 & 4) ? SPU_INT_ENABLE_SH4 : 0) |
        ((SPU_INT_TIMER & 4) ? SPU_INT_ENABLE_TIMER0 : 0) |
        ((SPU_INT_BUS & 4) ? SPU_INT_ENABLE_BUS : 0);
    SPU_REG32(REG_SPU_FIQ_BIT_1) =
        ((SPU_INT_SH4 & 2) ? SPU_INT_ENABLE_SH4 : 0) |
        ((SPU_INT_TIMER & 2) ? SPU_INT_ENABLE_TIMER0 : 0) |
        ((SPU_INT_BUS & 2) ? SPU_INT_ENABLE_BUS : 0);
    SPU_REG32(REG_SPU_FIQ_BIT_0) =
        ((SPU_INT_SH4 & 1) ? SPU_INT_ENABLE_SH4 : 0) |
        ((SPU_INT_TIMER & 1) ? SPU_INT_ENABLE_TIMER0 : 0) |
        ((SPU_INT_BUS & 1) ? SPU_INT_ENABLE_BUS : 0);

    /* Allow the SH4 to raise interrupts on the ARM */
    SPU_REG32(REG_SPU_INT_ENABLE) = SPU_INT_ENABLE_SH4 | SPU_INT_ENABLE_TIMER0;

    /* Allow the ARM to raise interrupts on the SH4 */
    SPU_REG32(REG_SPU_SH4_INT_ENABLE) = SPU_INT_ENABLE_SH4;
}

void aica_interrupt(void) {
    SPU_REG32(REG_SPU_SH4_INT_SEND) = SPU_INT_ENABLE_SH4;
}
