/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "pci.h"
#include "cobalt.h"
#include "galileo.h"
#include "version.h"

size_t ram_size, ram_restrict;

void loader(size_t bank0, size_t bank1, unsigned switches)
{
	extern char __text;

	unsigned long mark;
	int noshell;

	ram_size = bank0 + bank1;
	ram_restrict = ram_size;

	pci_init(bank0, bank1);

	if(switches & BUTTON_CLEAR)
		nv_get();
	else
		nv_put();

	if(!(nv_store.flags & NVFLAG_CONSOLE_DISABLE))
		serial_enable(1);

	puts("\n[ \"CoLo\" v" _STR(VER_MAJOR) "." _STR(VER_MINOR) " ]");

	printf("stage2: %08lx-%08lx\n", (unsigned long) &__text, (unsigned long) KSEG0(ram_size));

	printf("pci: unit type <%s>\n", pci_unit_name());

	tulip_init();

	ide_init();

	block_init();

	heap_reset();

	env_put("console-speed", serial_baud(), VAR_OTHER);

	if(!nv_store.boot || (switches & (BUTTON_ENTER | BUTTON_SELECT)) == 0)

		noshell = boot(0);

	else if(nv_store.flags & NVFLAG_CONSOLE_DISABLE)

		noshell = boot(-1);

	else {

		noshell = 0;

		for(mark = MFC0(CP0_COUNT); !BREAK();)
			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE * 3 / 2) {
				noshell = boot(-1);
				break;
			}
	}

	if(!noshell)
		serial_enable(1);

	shell();
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
