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
#define VER_MINOR					5

unsigned debug_flags;
size_t ram_size;

void loader(size_t bank0, size_t bank1, unsigned switches)
{
	unsigned long mark;

	serial_init();

	puts("\n-=<[ Qube2/RaQ2 boot loader v" _STR(VER_MAJOR) "." _STR(VER_MINOR) " ]>=-");

	ram_size = bank0 + bank1;

	pci_init(bank0, bank1);

	tulip_init();

	ide_init();

	block_init();

	for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE * 3 / 2;)
		if(kbhit() && getch() == ' ')
			shell(NULL);

	shell("mount\n-script /boot/default.boot\nload /boot/vmlinux.gz\nexecute");
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
