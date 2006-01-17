/*
 * (C) P.Horton 2004,2005,2006
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
 * change stack pointer and call function (with four arguments)
 *
 * this isn't necessary for calling the kernel, but it's handy
 * to be able to load and run other ELF images that can then
 * return to the shell
 *
 * arguments:
 *	uint64_t *	parameters
 *
 * parameters:
 *	uint64_t	new stack pointer
 *	uint64_t	function address
 *	uint64_t	arg #1
 *	uint64_t	arg #2
 *	uint64_t	arg #3
 *	uint64_t	arg #4
 */

launch:			ld	t9, 0(a0)	// switch to new stack
			move	t8, sp		
			dsubu	sp, t9, 64

			ld	t9, 8(a0)	// fetch parameters
			ld	a3, 40(a0)
			ld	a2, 32(a0)
			ld	a1, 24(a0)
			ld	a0, 16(a0)

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

			// do we need to do anything with unsaved
			// registers to ensure they work okay in
			// 32-bit mode (r0 for example) ?

			j	ra
			 nop

/* vi:set ts=8 sw=8 ai: */
