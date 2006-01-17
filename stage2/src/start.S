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

			.globl	_start

_start:			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			jal	loader
			 addiu	gp, %lo(_gp)

			b	.
			 nop

/* vi:set ts=8 sw=8 ai: */
