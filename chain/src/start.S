/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <regdefs.h>
#include <version.h>

#define __STR(x)	#x
#define _STR(x)		__STR(x)

			.text

			.set	mips4

			.set	noreorder

			.globl	_start

_start:			b	1f
			 nop

			.ascii	"CoLo"

			.ascii	" v", _STR(VER_MAJOR), ".", _STR(VER_MINOR)
			.asciiz	" (C) P.Horton 2004"

			.balign	4

1:			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			jal	chain
			 addiu	gp, %lo(_gp)

			b	.
			 nop

/* vi:set ts=8 sw=8 ai: */
