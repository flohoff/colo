/*
 * (C) P.Horton 2004
 *
 * $Id: start.S 4 2004-03-28 16:06:07Z pdh $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <regdefs.h>

			.text

			.set	mips4

			.set	noreorder

			.globl	_start

_start:			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			bal	chain
			 addiu	gp, %lo(_gp)

			b	.
			 nop

/* vi:set ts=8 sw=8 ai: */
