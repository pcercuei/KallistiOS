# KallistiOS ##version##
#
# task_asm.s
# Copyright (C) 2026 Paul Cercueil
#
# Low-level task handling

.text
.align

.globl	thd_block_now
.extern __fiq_stack
.extern thd_choose_new
.extern thd_return_from_exception

thd_block_now:
	# Disable interrupts
	mrs	r2, CPSR
	orr	r1, r2, #0xc0
	msr	CPSR_all, r1

	# Skip r0-r3 which are caller-saved
	add	r1, r0, #16

	# Save r4-r7 on the task structure
	stmia	r1,{r4-r7}

	# Save r8-r14 and the CPSR
	add	r0, r0, #36
	stmia	r0!, {r8-r14}
	str	r2, [r0]

	# Use the FIQ stack from now on
	ldr     sp,=__fiq_stack

	# Save r14 as PC
	str	r14, [r0,#-32]

	ldr	r14,=thd_return_from_exception

	b	thd_choose_new
	nop
