/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "linux/elf.h"

/*
 * validate ELF32 image
 */
int elf32_validate(const void *image, size_t imagesz, struct elf_info *info)
{
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;

	if(imagesz < sizeof(Elf32_Ehdr) || ((unsigned long) image & 3))
		return 0;

	eh = (Elf32_Ehdr *) image;

	if(eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3 ||
		eh->e_ident[EI_CLASS] != ELFCLASS32 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_ident[EI_VERSION] != EV_CURRENT ||
		eh->e_machine != EM_MIPS) {

		return 0;
	}

	if(!eh->e_phnum || eh->e_phentsize != sizeof(Elf32_Phdr))
		return 0;

	if(eh->e_phoff < sizeof(Elf32_Ehdr) || eh->e_phoff > imagesz || imagesz - eh->e_phoff < eh->e_phnum * sizeof(Elf32_Phdr))
		return 0;

	info->load_addr = 0xffffffff;
	info->load_size = 0;

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if(ph[indx].p_offset > imagesz || imagesz - ph[indx].p_offset < ph[indx].p_filesz)
				return 0;

			if(ph[indx].p_vaddr < info->load_addr)
				info->load_addr = ph[indx].p_vaddr;

			if(ph[indx].p_vaddr + ph[indx].p_memsz > info->load_addr + info->load_size)
				info->load_size = ph[indx].p_vaddr + ph[indx].p_memsz - info->load_addr;
		}

	if(info->load_addr & 3) {

		DPUTS("elf32: load address invalid");
		return 0;
	}

	if((eh->e_entry & 3) || eh->e_entry < info->load_addr || eh->e_entry >= info->load_addr + info->load_size) {

		DPUTS("elf32: entry point invalid");
		return 0;
	}

	info->entry_point = eh->e_entry;

	info->region_lo = 0;
	info->region_hi = (long) info->load_addr >> 31;

	DPRINTF("elf32: %08lx - %08lx (%08x:%08lx)\n",
			info->load_addr,
			info->load_addr + info->load_size - 1,
			info->region_hi,
			info->entry_point);

	/* map KSEG1 load address to KSEG0 */

	info->load_offset = 0;

	if(info->load_addr >= (unsigned long) KSEG1(0)) {

		info->load_offset = (long) KSEG0(0) - (long) KSEG1(0);

		DPRINTF("elf32: @%08lx\n", info->load_addr + info->load_offset);
	}

	return 1;
}

/*
 * load ELF32 image
 */
void elf32_load(const void *image, long offset)
{
	unsigned long vaddr;
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;

	eh = (Elf32_Ehdr *) image;

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			vaddr = ph[indx].p_vaddr + offset;

			DPRINTF("elf32: %08x (%08lx) <-- %08x %ut + %ut\n",
				vaddr - offset,
				vaddr,
				ph[indx].p_offset,
				ph[indx].p_filesz,
				ph[indx].p_memsz - ph[indx].p_filesz);

			memcpy((void *) vaddr, (void *) eh + ph[indx].p_offset, ph[indx].p_filesz);

			memset((void *) vaddr + ph[indx].p_filesz, 0, ph[indx].p_memsz - ph[indx].p_filesz);
		}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
