/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage1/src/start.S,v 1.3 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <regdefs.h>

			.text

			.set	mips4

			.set	noreorder

			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			bal	cpu_init
			 addiu	gp, %lo(_gp)

			bal	stage1
			 nop

			bal	loader
			 subu	sp, v0, 16

			b	fatal
			 nop

			.org	0x200

			# TLB refill

			li	a0, %lo(.)

1:			la	sp, __stack - 16
			lui	gp, %hi(_gp)
			b	exception
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
