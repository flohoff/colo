/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <regdefs.h>

			.text

			.set	mips4

			.set	noreorder

			.globl	launch


/*
 * change stack pointer and call function (with two arguments)
 *
 * this isn't necessary for calling the kernel, but it's handy
 * to be able to load and run other ELF images that can then
 * return to the shell
 *
 * arguments:
 *	uint64_t	new stack pointer
 *	uint64_t	function address
 *	uint64_t	arg #1
 *	uint64_t	arg #2
 */

launch:			move	t8, sp

			li	t9, 1
			dsll	t9, t9, 32
			dsubu	t9, t9, 1	// t9 - 0000.0000.ffff.ffff

			dsll	a1, a1, 32	// a1.a0 - new stack pointer
			dsll	a3, a3, 32	// a3.a2 - function address
			and	a0, a0, t9
			and	a2, a2, t9
			or	a0, a0, a1
			or	t9, a2, a3

			dsubu	sp, a0, 64	// load new stack pointer (allocating space for context)

			sw	t8, 52(sp)	// save context
			sw	ra, 60(sp)
			sw	fp, 56(sp)
			sw	gp, 48(sp)
			sw	s7, 44(sp)
			sw	s6, 40(sp)
			sw	s5, 36(sp)
			sw	s4, 32(sp)
			sw	s3, 28(sp)
			sw	s2, 24(sp)
			sw	s1, 20(sp)
			sw	s0, 16(sp)

			ld	a0, 16(t8)	// fetch arguments from original stack
			ld	a1, 24(t8)

			jal	t9		// go do the thing
			 move	gp, zero

			lw	s0, 16(sp)	// restore context
			lw	s1, 20(sp)
			lw	s2, 24(sp)
			lw	s3, 28(sp)
			lw	s4, 32(sp)
			lw	s5, 36(sp)
			lw	s6, 40(sp)
			lw	s7, 44(sp)
			lw	gp, 48(sp)
			lw	fp, 56(sp)
			lw	ra, 60(sp)
			lw	sp, 52(sp)

			j	ra
			 nop

/* vi:set ts=8 sw=8 ai: */
