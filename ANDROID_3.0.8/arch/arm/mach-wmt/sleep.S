/*++
linux/arch/arm/mach-wmt/sleep.s

Copyright (c) 2008  WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software Foundation,
either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/
#include <linux/linkage.h>
#include <asm/assembler.h>



/*
 * Registers access definitions
 */
/* Standard definitions of mode bits and interrupt (I & F) flags in PSRs */
#define	MODE_USR	0x10
#define MODE_FIQ	0x11
#define MODE_IRQ	0x12
#define MODE_SVC	0x13
#define MODE_ABT	0x17
#define MODE_UND	0x1B
#define MODE_SYS	0x1F

#define I_BIT		0x80
#define F_BIT		0x40

#define DO_POWER_OFF_SUSPEND	0xFFFFFFC4

	.arm

	.text

ENTRY(wmt_assem_suspend)
//suspend
	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_SVC
	msr	cpsr_c, r3			@ ensure we are in SVC mode
	mov	r2, sp                                  @ using svc stack pointer as the global one

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_FIQ
	msr	cpsr_c, r3			@ store FIQ bank registers
	mrs	r3, spsr
	stmfd	r2!, {r3, r8 - r14}

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_IRQ
	msr	cpsr_c, r3			@ store IRQ bank registers
	mrs	r3, spsr
	stmfd	r2!, {r3, r13, r14}

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_UND
	msr	cpsr_c, r3			@ store UND bank registers
	mrs	r3, spsr
	stmfd	r2!, {r3, r13, r14}

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_ABT
	msr	cpsr_c, r3			@ store ABT bank registers
	mrs	r3, spsr
	stmfd	r2!, {r3, r13, r14}

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_SYS
	msr	cpsr_c, r3                       @ store SYS bank registers
	mrs	r3, spsr
	stmfd	r2!, {r3 - r14}

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_SVC
	msr     cpsr_c, r3		@ come back to SVC mode
	mov	sp, r2			                @ restore the updated and used stack pointer

	ldreq r2, =0xfe000000
	
	mrs	r0, cpsr
	mrs	r1, spsr
	stmfd   sp!, {r0, r1, r4 - r12, lr}            	@ save current mode (SVC) registers on stack

	mrc	p15, 0, r4, c13, c0, 0	@ FCSE/PID
	mrc	p15, 0, r5, c13, c0, 1	@ Context ID
	mrc	p15, 0, r6, c13, c0, 3	@ User r/o thread ID
	
	stmfd	sp!, {r4 - r6}
	mrc	p15, 0, r6, c3, c0, 0	@ Domain ID
	mrc	p15, 0, r7, c2, c0, 0	@ TTB 0
	mrc	p15, 0, r8, c2, c0, 1	@ TTB 1
	mrc	p15, 0, r9, c1, c0, 0	@ Control register
	mrc	p15, 0, r10, c1, c0, 1	@ Auxiliary control register
	mrc	p15, 0, r11, c1, c0, 2	@ Co-processor access control
	stmfd	sp!, {r6 - r11}


	ldr r1, =0x130000
	add r5, r2, r1

	/* Store physcial address of stack to HSP2_REG */
	ldr	r1, =0xc0000000				@ PAGE_OFFSET, define in memory.h
	mov	r0, sp
	sub	r0, r0, r1                              @ __virt_to_phys(x)	((x) - PAGE_OFFSET + PHYS_OFFSET)
	str	r0, [r5, #0x34]				@ save sp to HSPR2

	/* Flush all data */

	dmb					@ ensure ordering with previous memory accesses
	mrc	p15, 1, r0, c0, c0, 1		@ read clidr
	ands	r3, r0, #0x7000000		@ extract loc from clidr
	mov	r3, r3, lsr #23			@ left align loc bit field
	beq	finished			@ if loc is 0, then no need to clean
	mov	r10, #0				@ start clean at cache level 0
loop1:
	add	r2, r10, r10, lsr #1		@ work out 3x current cache level
	mov	r1, r0, lsr r2			@ extract cache type bits from clidr
	and	r1, r1, #7			@ mask of the bits for current cache only
	cmp	r1, #2				@ see what cache we have at this level
	blt	skip				@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr
	isb					@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0		@ read the new csidr
	and	r2, r1, #7			@ extract the length of the cache lines
	add	r2, r2, #4			@ add 4 (line length offset)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3		@ find maximum number on the way size
	clz	r5, r4				@ find bit position of way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13		@ extract max number of the index size
loop2:
	mov	r9, r4				@ create working copy of max way size
loop3:
 ARM(	orr	r11, r10, r9, lsl r5	)	@ factor way and cache number into r11
 THUMB(	lsl	r6, r9, r5		)
 THUMB(	orr	r11, r10, r6		)	@ factor way and cache number into r11
 ARM(	orr	r11, r11, r7, lsl r2	)	@ factor index number into r11
 THUMB(	lsl	r6, r7, r2		)
 THUMB(	orr	r11, r11, r6		)	@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2		@ clean & invalidate by set/way
	subs	r9, r9, #1			@ decrement the way
	bge	loop3
	subs	r7, r7, #1			@ decrement the index
	bge	loop2
skip:
	add	r10, r10, #2			@ increment cache number
	cmp	r3, r10
	bgt	loop1
finished:
	mov	r10, #0				@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0		@ select current cache level in cssr


	
	dsb
	isb


	/* Save physical entry address */
	ldr     r1, =DO_POWER_OFF_SUSPEND
	
	/* Turn off MMU and disable caches */
	mrc	p15, 0, r0, c1, c0, 0	                @ read control reg
	bic	r0, r0, #0x05		                @ clear D-cache, turn off MMU
	bic	r0, r0, #0x1000		                @ clear I-cache
	mcr	p15, 0, r0, c1, c0, 0                   @ turn off ...
	nop
	//mov     pc, r1
	blx		r1
	nop
	nop
	nop
	nop
	

	ldmia   sp!, {r0, r1, r4 - r12, lr}

	msr	cpsr, r0
	msr	spsr, r1

	mov	r1, r2

	mov	r2, sp	                                @ using svc stack pointer as the global one

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_SYS
	msr	cpsr_c, r3
	ldmia	r2!, {r3 - r14}
	msr	spsr, r3

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_ABT
	msr	cpsr_c, r3			@ restore ABT bank registers
	ldmia	r2!, {r3, r13, r14}
	msr	spsr, r3

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_UND
	msr	cpsr_c, r3			@ restore UND bank registers
	ldmia	r2!, {r3, r13, r14}
	msr	spsr, r3

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_IRQ
	msr	cpsr_c, r3			@ restore IRQ bank registers
	ldmia	r2!, {r3, r13, r14}
	msr	spsr, r3

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_FIQ
	msr	cpsr_c, r3			@ restore FIQ bank registers
	ldmia	r2!, {r3, r8 - r14}
	msr	spsr, r3

	mrs	r3, cpsr
	bic r3, r3, #0x1f
	orr r3, r3, #MODE_SVC
	msr	cpsr_c,  r3 		@ ensure we are in SVC mode
	mov	sp, r2


	mov	pc, lr					@ return to caller, saved when get into suspend
      nop     	
      nop     	
      nop     
      nop      	

ENDPROC(wmt_assem_suspend)


