/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage2/src/pci.c,v 1.2 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "galileo.h"
#include "pci.h"

#define UNIT_ID_QUBE1			3
#define UNIT_ID_RAQ1				4
#define UNIT_ID_QUBE2			5
#define UNIT_ID_RAQ2				6

void pcicfg_write_word(unsigned dev, unsigned func, unsigned addr, unsigned data)
{
	assert(!(addr & 3));

	BRDG_REG_WORD(0xcf8) = 0x80000000 | (dev << 11) | (func << 8) | addr;
	BRDG_REG_WORD(0xcfc) = data;
}

void pcicfg_write_half(unsigned dev, unsigned func, unsigned addr, unsigned data)
{
	assert(!(addr & 1));

	BRDG_REG_WORD(0xcf8) = 0x80000000 | (dev << 11) | (func << 8) | addr;
	BRDG_REG_HALF(0xcfc | (addr & 3)) = data;
}

void pcicfg_write_byte(unsigned dev, unsigned func, unsigned addr, unsigned data)
{
	BRDG_REG_WORD(0xcf8) = 0x80000000 | (dev << 11) | (func << 8) | addr;
	BRDG_REG_BYTE(0xcfc | (addr & 3)) = data;
}

unsigned pcicfg_read_word(unsigned dev, unsigned func, unsigned addr)
{
	assert(!(addr & 3));

	BRDG_REG_WORD(0xcf8) = 0x80000000 | (dev << 11) | (func << 8) | addr;
	return BRDG_REG_WORD(0xcfc);
}

unsigned pcicfg_read_half(unsigned dev, unsigned func, unsigned addr)
{
	assert(!(addr & 1));

	BRDG_REG_WORD(0xcf8) = 0x80000000 | (dev << 11) | (func << 8) | addr;
	return BRDG_REG_HALF(0xcfc | (addr & 3));
}

unsigned pcicfg_read_byte(unsigned dev, unsigned func, unsigned addr)
{
	BRDG_REG_WORD(0xcf8) = 0x80000000 | (dev << 11) | (func << 8) | addr;
	return BRDG_REG_BYTE(0xcfc | (addr & 3));
}

unsigned pci_init(size_t bank0, size_t bank1)
{
	static const char *name[] = {
		[UNIT_ID_QUBE1]	= "Qube1",
		[UNIT_ID_RAQ1]		= "RaQ1",
		[UNIT_ID_QUBE2]	= "Qube2",
		[UNIT_ID_RAQ2]		= "RaQ2",
	};
	unsigned unit;

	/* set Galileo BARs for bus master memory access */

	BRDG_REG_WORD(BRDG_REG_RAS01_BANK_SIZE) = bank0 ? bank0 - 1 : 0;
	BRDG_REG_WORD(BRDG_REG_RAS23_BANK_SIZE) = bank1 ? bank1 - 1 : 0;

	if(bank0 >= bank1) {
		pcicfg_write_word(PCI_DEV_GALILEO, PCI_FNC_GALILEO, 0x10, 0);
		pcicfg_write_word(PCI_DEV_GALILEO, PCI_FNC_GALILEO, 0x14, bank0);
	} else {
		pcicfg_write_word(PCI_DEV_GALILEO, PCI_FNC_GALILEO, 0x10, bank1);
		pcicfg_write_word(PCI_DEV_GALILEO, PCI_FNC_GALILEO, 0x14, 0);
	}

	/* enable Galileo as bus master, and enable memory accesses */

	pcicfg_write_half(PCI_DEV_GALILEO, PCI_FNC_GALILEO, 0x04, 0x0006 |
		pcicfg_read_half(PCI_DEV_GALILEO, PCI_FNC_GALILEO, 0x04));

	/* read unit type */

	unit = pcicfg_read_byte(PCI_DEV_VIA, PCI_FNC_VIA_ISA, 0x94) >> 4;

	if(unit < elements(name) && name[unit])
		DPRINTF("pci: unit type \"%s\"\n", name[unit]);
	else
		DPRINTF("pci: unit type #%u\n", unit);

	return unit;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
