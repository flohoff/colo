/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _GALILEO_H_
#define _GALILEO_H_

#define BRDG_REG_WORD(o)					(((volatile uint32_t *) 0xb4000000)[(o)>>2])
#define BRDG_REG_HALF(o)					(((volatile uint16_t *) 0xb4000000)[(o)>>1])
#define BRDG_REG_BYTE(o)					(((volatile uint8_t *) 0xb4000000)[o])

#define BRDG_TCK								(50 * 1000000)

#define BRDG_PCI_BASE						0xb0000000
#define BRDG_NCS0_BASE						0xbc000000
#define BRDG_NCS1_BASE						0xbc800000
#define BRDG_NCS2_BASE						0xbd000000
#define BRDG_NCS3_BASE						0xbf000000
#define BRDG_NCSBOOT_BASE					0xbfc00000

#define BRDG_NCS0_CONFIG					0x1446db33	// byte wide - LEDs
#define BRDG_NCS1_CONFIG					0x144fe667	// byte wide - UART
#define BRDG_NCS2_CONFIG					0x1466db33	// word wide - switches
#define BRDG_NCS3_CONFIG					0x146fdffb	// word wide - LCD
#define BRDG_NCSBOOT_CONFIG				0x1446dc43	// byte wide - Flash

#define BRDG_ISA_SPACE						((volatile uint8_t *) BRDG_PCI_BASE)

#define BRDG_REG_RAS01_LOW_DECODE		0x008
#define BRDG_REG_RAS01_HIGH_DECODE		0x010
#define BRDG_REG_RAS23_LOW_DECODE		0x018
#define BRDG_REG_RAS23_HIGH_DECODE		0x020
#define BRDG_REG_RAS0_LOW_DECODE			0x400
#define BRDG_REG_RAS0_HIGH_DECODE		0x404
#define BRDG_REG_RAS1_LOW_DECODE			0x408
#define BRDG_REG_RAS1_HIGH_DECODE		0x40c
#define BRDG_REG_RAS2_LOW_DECODE			0x410
#define BRDG_REG_RAS2_HIGH_DECODE		0x414
#define BRDG_REG_RAS3_LOW_DECODE			0x418
#define BRDG_REG_RAS3_HIGH_DECODE		0x41c
#define BRDG_REG_DRAM_CONFIG				0x448
#define BRDG_REG_DRAM_PARM_0				0x44c
#define BRDG_REG_DRAM_PARM_1				0x450
#define BRDG_REG_DRAM_PARM_2				0x454
#define BRDG_REG_DRAM_PARM_3				0x458
#define BRDG_REG_DEV_PARM_NCS0			0x45c
#define BRDG_REG_DEV_PARM_NCS1			0x460
#define BRDG_REG_DEV_PARM_NCS2			0x464
#define BRDG_REG_DEV_PARM_NCS3			0x468
#define BRDG_REG_DEV_PARM_NCSBOOT		0x46c
#define BRDG_REG_COUNTER_0					0x850
#define BRDG_REG_COUNTER_CTRL				0x864
#define BRDG_REG_TIMEOUT_RETRY			0xc04
#define BRDG_REG_RAS01_BANK_SIZE			0xc08
#define BRDG_REG_RAS23_BANK_SIZE			0xc0c
#define BRDG_REG_INTR_CAUSE				0xc18
# define BRDG_INTR_CAUSE_RETRY_CTR		(1 << 20)

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
