/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _PCI_H_
#define _PCI_H_

#define PCI_CLOCK								(33 * 1000000)

#define PCI_DEV_SLOT							10
#define PCI_FNC_SLOT							0

#define PCI_DEV_GALILEO						0
#define PCI_FNC_GALILEO						0

#define PCI_DEV_ETH0							7
#define PCI_FNC_ETH0							0

#define PCI_DEV_VIA							9
#define PCI_FNC_VIA_ISA						0
#define PCI_FNC_VIA_IDE						1

#define PCI_DEV_ETH1							12
#define PCI_FNC_ETH1							0

extern unsigned pcicfg_read_word(unsigned, unsigned, unsigned);
extern unsigned pcicfg_read_half(unsigned, unsigned, unsigned);
extern unsigned pcicfg_read_byte(unsigned, unsigned, unsigned);
extern void pcicfg_write_word(unsigned, unsigned, unsigned, unsigned);
extern void pcicfg_write_half(unsigned, unsigned, unsigned, unsigned);
extern void pcicfg_write_byte(unsigned, unsigned, unsigned, unsigned);

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
