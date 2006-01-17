/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "linux/elf.h"

/*
 * convert mapped address to physical
 */
static long phys_addr(unsigned long long addr)
{
	if(~double_word_hi(addr)) {

		if((double_word_hi(addr) & 0xc7ffffff) == 0x80000000 && (long) double_word_lo(addr) >= 0)
			return double_word_lo(addr);

	} else if((double_word_lo(addr) >> 30) == 2)

		return (long) KPHYS(double_word_lo(addr));

	return -1;
}

/*
 * validate ELF64 image
 */
int elf64_validate(const void *image, size_t imagesz, struct elf_info *info)
{
	unsigned indx, phoff, offset, filesz, memsz;
	unsigned long phys;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;

	if(imagesz < sizeof(Elf64_Ehdr) || ((unsigned long) image & 7))
		return 0;

	eh = (Elf64_Ehdr *) image;

	if(eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3 ||
		eh->e_ident[EI_CLASS] != ELFCLASS64 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_ident[EI_VERSION] != EV_CURRENT ||
		eh->e_machine != EM_MIPS) {

		return 0;
	}

	phoff = eh->e_phoff;

	if(double_word_hi(eh->e_phoff) ||
		!eh->e_phnum ||
		eh->e_phentsize != sizeof(Elf64_Phdr) ||
		phoff < sizeof(Elf64_Ehdr) ||
		phoff > imagesz ||
		imagesz - phoff < eh->e_phnum * sizeof(Elf64_Phdr)) {

		return 0;
	}

	ph = (void *) eh + phoff;

	info->load_phys = 0xffffffff;
	info->load_size = 0;
	info->data_sect = SIGN_EXTEND_64(KSEG0(0));

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if(double_word_hi(ph[indx].p_offset) ||
				double_word_hi(ph[indx].p_filesz) ||
				double_word_hi(ph[indx].p_memsz)) {

				return 0;
			}

			phys = phys_addr(ph[indx].p_vaddr);

			if((long) phys < 0 || (phys & 3))
				return 0;

			offset = ph[indx].p_offset;
			filesz = ph[indx].p_filesz;
			memsz = ph[indx].p_memsz;

			if(offset > imagesz || imagesz - offset < filesz)
				return 0;

			if(phys < info->load_phys)
				info->load_phys = phys;

			if(phys + memsz > info->load_phys + info->load_size)
				info->load_size = phys + memsz - info->load_phys;

			info->data_sect = ph[indx].p_vaddr - phys;
		}

	phys = phys_addr(eh->e_entry);

	if((long) phys < 0 || (phys & 3) || phys < info->load_phys || phys > info->load_phys + info->load_size)
		return 0;

	info->entry_point = eh->e_entry;

	DPRINTF("elf64: %08lx - %08lx (%08x.%08x) (%08x.%08x)\n",
		info->load_phys,
		info->load_phys + info->load_size - 1,
		double_word_hi(info->entry_point),
		double_word_lo(info->entry_point),
		double_word_hi(info->data_sect),
		double_word_lo(info->data_sect));

	return 1;
}

/*
 * load ELF64 image
 */
void elf64_load(const void *image)
{
	unsigned indx, phoff, filesz, memsz;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;
	void *vaddr;

	eh = (Elf64_Ehdr *) image;

	phoff = eh->e_phoff;

	ph = (void *) eh + phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			vaddr = KSEG0(phys_addr(ph[indx].p_vaddr));
			filesz = ph[indx].p_filesz;
			memsz = ph[indx].p_memsz;

			memcpy(vaddr, (void *) eh + (unsigned long) ph[indx].p_offset, filesz);
			memset(vaddr + filesz, 0, memsz - filesz);

			DPRINTF("elf64: %08x.%08x (%08lx) %ut + %ut\n",
					double_word_hi(ph[indx].p_vaddr),
					double_word_lo(ph[indx].p_vaddr),
					(unsigned long) vaddr,
					filesz,
					memsz - filesz);
		}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
