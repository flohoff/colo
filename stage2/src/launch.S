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
 * change stack pointer and call function (with four arguments)
 *
 * this isn't necessary for calling the kernel, but it's handy
 * to be able to load and run other ELF images that can then
 * return to the shell
 */

launch:			move	t8, sp
			move	t9, a1
			subu	sp, a0, 64

			sw	t8, 52(sp)
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

			move	a0, a2
			move	a1, a3
			lw	a2, 16(t8)
			lw	a3, 20(t8)

			jal	t9
			 move	gp, zero

			lw	s0, 16(sp)
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
