/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "pci.h"
#include "galileo.h"

#define IO_BASE_ETH0						0x10100000
#define IO_BASE_ETH1						0x10101000

#define TULIP_VND_ID						0x1011
#define TULIP_DEV_ID						0x0019

static void tulip_setup(unsigned dev, unsigned fnc, unsigned iob)
{
	if(pcicfg_read_word(dev, fnc, 0x00) == ((TULIP_DEV_ID << 16) | TULIP_VND_ID))
		pcicfg_write_word(dev, fnc, 0x10, iob);
}

void tulip_init(void)
{
	tulip_setup(PCI_DEV_ETH0, PCI_FNC_ETH0, IO_BASE_ETH0);
	tulip_setup(PCI_DEV_ETH1, PCI_FNC_ETH1, IO_BASE_ETH1);

	/* without this Tulip bus mastering doesn't work correctly */

	BRDG_REG_WORD(BRDG_REG_TIMEOUT_RETRY) = 0xffff;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
