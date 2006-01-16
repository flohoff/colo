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

static int do_boot(unsigned switches)
{
	unsigned long mark;

	if(!(switches & (BUTTON_ENTER | BUTTON_SELECT)))
		return boot(BOOT_MENU);

	if(!(nv_store.flags & NVFLAG_CONSOLE_DISABLE))
		for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 2 / 3;)
			if(BREAK())
				return E_NONE;

	return boot(BOOT_DEFAULT);
}

void loader(size_t bank0, size_t bank1, unsigned switches)
{
	extern char __text;
	unsigned clock;

	ram_size = bank0 + bank1;
	ram_restrict = ram_size;

	pci_init(bank0, bank1);

	nv_get(!(switches & BUTTON_CLEAR));

	if(!(nv_store.flags & NVFLAG_CONSOLE_DISABLE))
		serial_enable(1);

	puts("\n[ \"CoLo\" v" _STR(VER_MAJOR) "." _STR(VER_MINOR) " ]");

	printf("stage2: %08lx-%08lx\n", (unsigned long) &__text, (unsigned long) KSEG0(ram_size));

	clock = cpu_clock_khz();

	printf("cpu: clock %u.%03uMHz\n", clock / 1000, clock % 1000);

	printf("pci: unit type <%s>\n", pci_unit_name());

	tulip_init();

	ide_init();

	block_init();

	heap_reset();

	env_put("unit-type", pci_unit_name(), VAR_OTHER);

	if(do_boot(switches) != E_NONE) {

		lcd_line(0, "!  UNIT BOOT   !");
		lcd_line(1, "!   FAILED     !");

	} else {

		lcd_line(0, "Boot shell...");
		lcd_line(1, NULL);
	}

	if(!netcon_enabled())
		serial_enable(1);

	shell();
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
