/*
 * (C) P.Horton 2004,2005,2006
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

			b	1f
			 nop

			.ascii	"CoLo"

			.ascii	" v", _STR(VER_MAJOR), ".", _STR(VER_MINOR)
			.asciiz	" (C) P.Horton 2004,2005,2006"

			.balign	4

1:			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			jal	cpu_init
			 addiu	gp, %lo(_gp)

			jal	stage1
			 nop

			jal	loader
			 subu	sp, v0, 16

			j	fatal
			 nop

			.org	0x200

			# TLB refill

			li	a0, %lo(.)

1:			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			j	exception
			 addiu	gp, %lo(_gp)

			.org	0x280

			# XTLB refill

2:			b	1b
			 li	a0, %lo(2b)

			.org	0x300

			# Cache error

2:			b	1b
			 li	a0, %lo(2b)

			.org	0x380

			# General exception

2:			b	1b
			 li	a0, %lo(2b)

			.org	0x400

			# Interrupt (IV set in CP0 CAUSE)

2:			b	1b
			 li	a0, %lo(2b)

/* vi:set ts=8 sw=8 ai: */
