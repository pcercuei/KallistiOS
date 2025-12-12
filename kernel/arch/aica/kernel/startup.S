# KallistiOS ##version##
#
# startup.S
# Copyright (C) 2026 Paul Cercueil
#
# Startup for ARM program

.text
.section .text.init

.globl	thd_return_from_exception
.extern	fiq_handler
.extern	irq_srt_addr
.extern	arch_main
.extern	__stack
.extern	__fiq_stack

# Meaningless but makes the linker shut up
.globl	start
start:

	# Exception vectors
	b	1f
	mov	pc,r14		// Undef
	mov	pc,r14		// Softint
	sub	pc,r14,#4	// Prefetch abort
	sub	pc,r14,#4	// Data abort
	sub	pc,r14,#4	// Reserved instruction
	sub	pc,r14,#4	// IRQ

	# Beginning of FIQ exception routine

	# Disable IRQs/FIQs
	mrs	r8, CPSR
	orr	r9, r8, #0xc0
	msr	CPSR_all, r9

	# Correct old PC's offset
	sub	r14, r14, #4

	# Save r0-r7 and r14 (old PC) inside the thread structure
	ldr	r9, =irq_srt_addr
	ldr	r9, [r9]
	stmia	r9, {r0-r7,r14}

	# Save the banked CPSR for later
	mrs	r1, SPSR_all

	# Switch to Supervisor mode
	# and disable interrupts
	mrs	r9, CPSR
	orr	r9, r9, #0xd3
	msr	CPSR_all, r9
	nop
	nop
	nop
	nop

	# Save r8-r14 and the CPSR
	ldr	r4, =irq_srt_addr
	ldr	r4, [r4]
	add	r4, r4, #36
	stmia	r4!, {r8-r14}
	str	r1, [r4]

	# Use the FIQ stack for the moment
	ldr	sp,=__fiq_stack

	# Jump to the C handler
	bl	fiq_handler

thd_return_from_exception:
	# Restore r8-r14
	add	r1, r0, #36
	ldmia	r1, {r8-r14}

	# Switch to FIQ mode, to mask r8-r14
	mrs	r1, CPSR
	bic	r1, r1, #0x1f
	orr	r1, r1, #0x11
	msr	CPSR_all, r1
	nop
	nop
	nop
	nop

	# Restore the CPSR
	ldr	r8, [r0, #64]
	msr	SPSR_all, r8

	# Restore r0-r7 and PC
	mov	r8, r0
	ldmia	r8, {r0-r7, r14}

	# Jump to the new task
	add	r14, r14, #4
	subs	pc, r14, #4

1:
	# Setup a temporary stack
	ldr	sp,=__stack

	# Call the main for the SPU
	b	arch_main
