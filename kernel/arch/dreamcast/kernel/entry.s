! KallistiOS ##version##
!
!   arch/dreamcast/kernel/entry.s
!   Copyright (C) 2000, 2001 Megan Potter
!   Copyright (C) 2023 Paul Cercueil <paul@crapouillou.net>
!   Copyright (C) 2025 Falco Girgis
!
! Assembler code for entry and exit to/from the kernel via exceptions
!

! Routine that all exception handlers jump to after setting
! an error code. Out of necessity, all of this function is in
! assembler instead of C. Once the registers are saved, we will
! jump into a shared routine. This register save and restore code
! is mostly from my sh-stub code. For now this is pretty high overhead
! for a context switcher (or especially a timer! =) but it can
! be optimized later.

	.text
	.align		2
	.globl		_irq_srt_addr
	.globl		_irq_handle_exception
	.globl		_vma_table_100

! Static kernel-mode stack; we can get away with this because in our
! tiny microkernel, only one thread will ever actually be sitting inside
! the kernel code. All other threads will be halted at the point at which
! they made their trap call into the kernel. This lets us do all sorts of
! useful things, like freedom to remap memory at any time, safety from
! mis-mapped user-mode stack pointers, etc. It also opens the door for
! hard real-time interrupts and exceptions that interrupt the kernel
! itself.
	.space		4096		! One page
krn_stack:

! The SH4 has very odd exception handling. Instead of having a vector
! table like a sensible processor, it has a vector code block. *sigh*
! Thus this table of assembly code. Note that we can't catch reset
! exceptions at all, but that really shouldn't matter.
	.text
	.align 2

_vma_table_100:		! General exceptions
	mov	#1,r4			! Set exception code
_vma_hdl:
	mov.l	_irq_srt_addr,r5	! Grab the location of the reg store
	mov.l	hdl_except,r2		! Call handle_exception
	add	#0x20, r5
	sts.l	fpscr,@-r5
	sts.l	fpul,@-r5		! save FPUL  0x18
	stc.l	ssr,@-r5		! save SSR
	sts.l	macl,@-r5		! save MACL
	sts.l	mach,@-r5		! save MACH
	stc.l	gbr,@-r5		! save GBR
	sts.l	pr,@-r5			! save PR
	stc.l	spc,@-r5		! save PC    0x00

	mov.l	stkaddr,r15		! Switch to IRQ stack

	! R4 still contains the exception code
	jsr	@r2			! Call handle_exception
	nop

	mov.l	_irq_srt_addr,r2	! Get new register store address
	cmp/eq	r0,r2
	ldc.l	@r2+,spc		! restore SPC 0x00
	lds.l	@r2+,pr			! restore PR
	ldc.l	@r2+,gbr		! restore GBR
	lds.l	@r2+,mach		! restore MACH
	lds.l	@r2+,macl		! restore MACL
	ldc.l	@r2+,ssr		! restore SSR  0x14
	lds.l	@r2+,fpul		! restore FPUL 0x18

	bf/s	2f
	mov.l	@r2+,r3			! load FPSCR 0x1c

	stc	sgr,r15			! Restore R15 from SGR

1:
	lds	r3,fpscr		! restore FPSCR
	rte				! return
	nop

2:
	! A different thread has been scheduled.
	! We need to save the previous thread's registers, and load the
	! new ones.

	add	#0x70,r0
	add	#0x70,r0

	stc	sr,r15

	stc.l	sgr,@-r0		! save R15
	mov.l	r14,@-r0		! save R14   0xd8
	mov	#0x20,r14
	mov.l	r13,@-r0		! save R13   0xd4
	shll16	r14
	mov.l	r12,@-r0		! save R12
	shll8	r14
	mov.l	r11,@-r0		! save R11
	xor	r15,r14
	mov.l	r10,@-r0		! save R10
	mov	r2,r12
	mov.l	r9,@-r0			! save R9
	add	#0x40,r12
	mov.l	r8,@-r0			! save R8
	add	#0x40,r12

	mov	r0,r13

	ldc	r14,sr			! Swap R0-R7 banks

	mov.l	r7,@-r13		! save R7
	mov.l	r6,@-r13		! save R6
	mov.l	r5,@-r13		! save R5
	mov.l	r4,@-r13		! save R4
	mov.l	r3,@-r13		! save R3
	mov.l	r2,@-r13		! save R2
	mov.l	r1,@-r13		! save R1
	mov.l	r0,@-r13		! save R0

	mov.l	@r12+,r0		! load R0
	mov.l	@r12+,r1		! load R1
	mov.l	@r12+,r2		! load R2
	mov.l	@r12+,r3		! load R3
	mov.l	@r12+,r4		! load R4
	mov.l	@r12+,r5		! load R5
	mov.l	@r12+,r6		! load R6
	mov.l	@r12+,r7		! load R7

	ldc	r15,sr			! Swap back R0-R7 banks

	mov	r12,r2
	mov.l	@r2+,r8			! restore R8
	mov	r13,r0
	mov.l	@r2+,r9			! restore R9
	mov	#0x30,r4		! Set bits 21-20 to r4
	mov.l	@r2+,r10		! restore R10
	shll16	r4
	mov.l	@r2+,r11		! restore R11
	mov.l	@r2+,r12		! restore R12
	mov.l	@r2+,r13		! restore R13
	mov.l	@r2+,r14		! restore R14
	mov.l	@r2+,r15		! restore R15

	lds	r4,fpscr		! Switch to FPU bank 2, 64-bit I/O

	fmov	dr14,@-r0		! Save FR15/FR14  0x98
	fmov	dr12,@-r0		! Save FR13/FR12
	fmov	dr10,@-r0		! Save FR11/FR10
	fmov	dr8,@-r0		! Save FR9/FR8
	fmov	dr6,@-r0		! Save FR7/FR6
	fmov	dr4,@-r0		! Save FR5/FR4
	fmov	dr2,@-r0		! Save FR3/FR2
	fmov	dr0,@-r0		! Save FR1/FR0    0x60
	frchg				! Switch back to first bank

	fmov	dr14,@-r0		! Save FR15/FR14  0x58
	add	#-0x60,r2
	fmov	dr12,@-r0		! Save FR13/FR12
	add	#-0x60,r2
	fmov	dr10,@-r0		! Save FR11/FR10
	fmov	dr8,@-r0		! Save FR9/FR8
	fmov	dr6,@-r0		! Save FR7/FR6
	fmov	dr4,@-r0		! Save FR5/FR4
	fmov	dr2,@-r0		! Save FR3/FR2
	fmov	dr0,@-r0		! Save FR1/FR0    0x20

	fmov	@r2+,dr0		! restore FR0/FR1    0x20
	fmov	@r2+,dr2		! restore FR2/FR3
	fmov	@r2+,dr4		! restore FR4/FR5
	fmov	@r2+,dr6		! restore FR6/FR7
	fmov	@r2+,dr8		! restore FR8/FR9
	fmov	@r2+,dr10		! restore FR10/FR11
	fmov	@r2+,dr12		! restore FR12/FR13
	fmov	@r2+,dr14		! restore FR14/FR15  0x58
	frchg				! Second FP bank

	fmov	@r2+,dr0		! restore FR0/FR1    0x60
	fmov	@r2+,dr2		! restore FR2/FR3
	fmov	@r2+,dr4		! restore FR4/FR5
	fmov	@r2+,dr6		! restore FR6/FR7
	fmov	@r2+,dr8		! restore FR8/FR9
	fmov	@r2+,dr10		! restore FR10/FR11
	fmov	@r2+,dr12		! restore FR12/FR13
	fmov	@r2+,dr14		! restore FR14/FR15  0x98

	bra	1b
	nop

	.align 2
irqd_and:
	.long	0xefffff0f
irqd_or:
	.long	0x000000f0
_irqfr_or:
	.long	0x20000000
stkaddr:
	.long	krn_stack
_irq_srt_addr:
	.long	0	! Save Regs Table -- this is an indirection
			! so we can easily swap out pointers during a
			! context switch.
hdl_except:
	.long	_irq_handle_exception

	.rep 0x300 - 260
	.byte	0
	.endr

_vma_table_400:		! TLB miss exceptions (MMU)
	nop
	bra	_vma_hdl
	mov	#2,r4			! Set exception code

	.rep	0x200 - 6
	.byte	0
	.endr

_vma_table_600:		! IRQs
	nop
	bra	_vma_hdl
	mov	#3,r4			! Set exception code
