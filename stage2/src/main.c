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
#include "galileo.h"

#define VER_MAJOR					1
#define VER_MINOR					8

unsigned debug_flags;
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

	tulip_init();

	ide_init();

	block_init();

	heap_reset();

	for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 3 / 2;)
		if(kbhit() && getch() == ' ')
			shell(NULL);

	shell("mount\n-script /boot/default.colo\nload /boot/vmlinux.gz\nexecute");
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
