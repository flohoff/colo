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

			.globl	__start
__start:		 bal     1f
			 nop
			.word   _gp

1:			lw      gp, 0(ra)
			la	sp, __stack - 16

			la	t9,loader
			jalr	t9
			 nop

			b	.
			 nop

/* vi:set ts=8 sw=8 ai: */
