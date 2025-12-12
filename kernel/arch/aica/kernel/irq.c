#include <kos/irq.h>
#include <kos/regfield.h>

#include <arch/arch.h>
#include <arch/timer.h>
#include <aica/queue.h>
#include <aica/registers.h>

#include <stdbool.h>
#include <string.h>

extern timer_primary_callback_t tp_callback;

irq_context_t *irq_srt_addr;
int inside_int;

static irq_cb_t irq_handlers[0x10];

static irq_cb_t dft_handler;

int arch_irq_set_global_handler(irq_hdl_t hnd, void *data) {
    dft_handler = (irq_cb_t){
        .hdl = hnd,
        .data = data,
    };

    return 0;
}

irq_cb_t arch_irq_get_global_handler(void) {
    return dft_handler;
}

int arch_irq_set_handler(irq_t code, irq_hdl_t hnd, void *data) {
    irq_disable_scoped();

    irq_handlers[code] = (irq_cb_t){ hnd, data };

    return 0;
}

irq_cb_t arch_irq_get_handler(irq_t code) {
    return irq_handlers[code];
}

/* Called from startup.S */
irq_context_t *fiq_handler(void)
{
    const irq_cb_t *hnd;
    unsigned int req = SPU_REG32(REG_SPU_INT_REQUEST);
    irq_t code = FIELD_GET(req, SPU_INT_REQUEST_CODE);
    bool handled = false;

    inside_int = 1;

    if(dft_handler.hdl) {
        dft_handler.hdl(code, irq_srt_addr, dft_handler.data);
        handled = true;
    }

    hnd = &irq_handlers[code];
    if(hnd->hdl != NULL) {
        hnd->hdl(code, irq_srt_addr, hnd->data);
        handled = true;
    }

    if(!handled)
        arch_panic("unhandled IRQ/Exception");

    inside_int = 0;

    /* ACK FIQ interrupt */
    SPU_REG32(REG_SPU_INT_CLEAR) = 1;

    return irq_srt_addr;
}

void irq_interrupt_sh4(void) {
    SPU_REG32(REG_SPU_SH4_INT_SEND) = SPU_INT_ENABLE_SH4;
}

int irq_init(void) {
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

    /* Ack FIQ interrupt if there is one */
    SPU_REG32(REG_SPU_INT_CLEAR) = 1;

    /* Ack SH4 interrupt if there is one */
    SPU_REG32(REG_SPU_INT_RESET) = SPU_INT_ENABLE_SH4;

    /* Allow the SH4 and timer to raise interrupts on the ARM */
    SPU_REG32(REG_SPU_INT_ENABLE) = SPU_INT_ENABLE_SH4 | SPU_INT_ENABLE_TIMER0;

    /* Allow the ARM to raise interrupts on the SH4 */
    SPU_REG32(REG_SPU_SH4_INT_ENABLE) = SPU_INT_ENABLE_SH4;

    return 0;
}

void irq_shutdown(void) {
    /* Forbid interrupts to the SH4 */
    SPU_REG32(REG_SPU_SH4_INT_ENABLE) = 0;

    /* Mask all interrupts */
    SPU_REG32(REG_SPU_INT_ENABLE) = 0;
}

void arch_irq_create_context(irq_context_t *context,
                             uintptr_t stack_pointer,
                             uintptr_t routine,
                             const uintptr_t *args) {
    /* Clear out all registers. */
    memset(context, 0, sizeof(*context));

    /* Setup the program frame */
    context->pc = (uint32_t)routine;
    context->r8_r14[5] = stack_pointer;
    context->r8_r14[3] = 0xffffffff;
    context->cpsr = 0x13; // Supervisor mode

    /* Copy up to four args */
    context->r0_r7[0] = args[0];
    context->r0_r7[1] = args[1];
    context->r0_r7[2] = args[2];
    context->r0_r7[3] = args[3];
}

void arch_irq_set_context(irq_context_t *regbank) {
    irq_srt_addr = regbank;
}

irq_context_t *arch_irq_get_context(void) {
    return irq_srt_addr;
}
