# KallistiOS ##version##
#
#  crt0.s
#  (c)2000-2002 Megan Potter
#  Copyright (C) 2024 Paul Cercueil
#
#  Startup for ARM program

.text
.section .text.init

.extern	arm_main
.extern	fiq_handler

# Meaningless but makes the linker shut up
.globl	reset
reset:

	# Exception vectors
	b	start
	mov	pc,r14		// Undef
	mov	pc,r14		// Softint
	sub	pc,r14,#4	// Prefetch abort
	sub	pc,r14,#4	// Data abort
	sub	pc,r14,#4	// Reserved instruction
	sub	pc,r14,#4	// IRQ

	# Use the FIQ stack for the moment
	ldr sp,=__fiq_stack

	# Jump to the C handler
	b fiq_handler

start:
	# Setup a basic stack
	ldr	sp,=__stack

	# Clear BSS section
	ldr     r2,=__bss_end
	ldr     r1,=__bss_start
	mov     r0,#0

clear_bss_loop:
	str	r0,[r2,#-4]!
	cmp	r2,r1
	bhi	clear_bss_loop

	# Call the main for the SPU
	b	arm_main
