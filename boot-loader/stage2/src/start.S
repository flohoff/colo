/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage2/src/start.S,v 1.3 2004/02/15 12:45:18 pdh Exp $
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
			jal	loader
			 addiu	gp, %lo(_gp)

			b	.
			 nop

/* vi:set ts=8 sw=8 ai: */
