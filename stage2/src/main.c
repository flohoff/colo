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

size_t ram_size;

void loader(size_t bank0, size_t bank1, unsigned switches)
{
	extern char __text;

	unsigned long mark;

	serial_init();

	puts("\n-=<[ \"CoLo\" Qube2/RaQ2 boot loader v" _STR(VER_MAJOR) "." _STR(VER_MINOR) " (" __DATE__ ") ]>=-");

	ram_size = bank0 + bank1;

	printf("stage2: %08lx-%08lx\n", (unsigned long) &__text, (unsigned long) KSEG0(ram_size));

	pci_init(bank0, bank1);

	if(switches & BUTTON_CLEAR)
		nv_get();

	tulip_init();

	ide_init();

	block_init();

	heap_reset();

	env_put("console-speed", _STR(BAUD_RATE), VAR_OTHER);

	if(!nv_store.boot || (switches & (BUTTON_ENTER | BUTTON_SELECT)) == 0)

		boot(0);

	else

		for(mark = MFC0(CP0_COUNT); !BREAK();)
			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE * 3 / 2) {
				boot(-1);
				break;
			}

	shell();
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
