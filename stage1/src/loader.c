/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

typedef unsigned short		__u16;
typedef unsigned				__u32;
typedef unsigned long long	__u64;

typedef short					__s16;
typedef int						__s32;
typedef long long				__s64;

#include "linux/elf.h"

static void *(*memcpy_w)(void *, const void *, size_t)	= _memcpy_w;
static void *(*memset_w)(void *, int, size_t)				= _memset_w;

/*
 * flush entire D-cache
 */
static void dcache_flush_all(void)
{
	void *line, *end;

	line = KSEG0(0);

	for(end = line + DCACHE_TOTAL_SIZE; line < end; line += DCACHE_LINE_SIZE)
		CACHE(CACHE_IndexWritebackInvD, line);
}

/*
 * load boot loader from ELF image in Flash
 */
void loader(void)
{
	extern char __stage2;
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;

	/* ensure _memcpy_w() / _memset_w() are in physical memory */

	dcache_flush_all();

	eh = KSEG1(&__stage2);

	if(eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3 ||
		eh->e_ident[EI_CLASS] != ELFCLASS32 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_ident[EI_VERSION] != EV_CURRENT ||
		eh->e_machine != EM_MIPS ||
		!eh->e_phoff ||
		!eh->e_phnum ||
		eh->e_phentsize != sizeof(Elf32_Phdr))
	{
		lcd_line(0, "!ELF LOAD FAIL !");
		lcd_line(1, "  BAD HEADER");
		fatal();
	}

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if((ph[indx].p_vaddr & 3) ||
				(ph[indx].p_offset & 3) ||
				(ph[indx].p_filesz & 3) ||
				(ph[indx].p_memsz & 3))
			{
				lcd_line(0, "!ELF LOAD FAIL !");
				lcd_line(1, "  BAD SECTION");
				fatal();
			}

			memcpy_w((void *) ph[indx].p_vaddr, (void *) eh + ph[indx].p_offset, ph[indx].p_filesz);
			memset_w((void *) ph[indx].p_vaddr + ph[indx].p_filesz, 0, ph[indx].p_memsz - ph[indx].p_filesz);
		}

	/* ensure what we just loaded is in physical memory */

	dcache_flush_all();

	/* jump to it */

	((void (*)(size_t, size_t, unsigned)) eh->e_entry)(mem_bank[0], mem_bank[1], switches);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
