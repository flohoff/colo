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
 * convert mapped address to physical
 */
static long phys_addr(unsigned long addr)
{
	return (addr >> 30) == 2 ? (long) KPHYS(addr) : -1;
}

/*
 * validate ELF32 image
 */
int elf32_validate(const void *image, size_t imagesz, struct elf_info *info)
{
	unsigned long phys, base;
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

	if(!eh->e_phnum ||
		eh->e_phentsize != sizeof(Elf32_Phdr) ||
		eh->e_phoff < sizeof(Elf32_Ehdr) ||
		eh->e_phoff > imagesz ||
		imagesz - eh->e_phoff < eh->e_phnum * sizeof(Elf32_Phdr)) {

		return 0;
	}

	info->load_phys = 0xffffffff;
	info->load_size = 0;

	base = (unsigned long) KSEG0(0);

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if(ph[indx].p_offset > imagesz || imagesz - ph[indx].p_offset < ph[indx].p_filesz)
				return 0;

			phys = phys_addr(ph[indx].p_vaddr);

			if((long) phys < 0 || (phys & 3))
				return 0;

			if(phys < info->load_phys)
				info->load_phys = phys;

			if(phys + ph[indx].p_memsz > info->load_phys + info->load_size)
				info->load_size = phys + ph[indx].p_memsz - info->load_phys;

			base = ph[indx].p_vaddr - phys;
		}

	phys = phys_addr(eh->e_entry);

	if((long) phys < 0 || (phys & 3) || phys < info->load_phys || phys > info->load_phys + info->load_size)
		return 0;

	info->entry_point = SIGN_EXTEND_64(eh->e_entry);

	info->data_sect = SIGN_EXTEND_64(base);

	DPRINTF("elf32: %08lx - %08lx (%08x) (%08x.%08x)\n",
		info->load_phys,
		info->load_phys + info->load_size - 1,
		eh->e_entry,
		double_word_hi(info->data_sect),
		double_word_lo(info->data_sect));

	return 1;
}

/*
 * load ELF32 image
 */
void elf32_load(const void *image)
{
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;
	void *vaddr;

	eh = (Elf32_Ehdr *) image;

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			vaddr = KSEG0(phys_addr(ph[indx].p_vaddr));

			memcpy(vaddr, (void *) eh + ph[indx].p_offset, ph[indx].p_filesz);
			memset(vaddr + ph[indx].p_filesz, 0, ph[indx].p_memsz - ph[indx].p_filesz);

			DPRINTF("elf32: %08x (%08lx) %ut + %ut\n",
					ph[indx].p_vaddr,
					(unsigned long) vaddr,
					ph[indx].p_filesz,
					ph[indx].p_memsz - ph[indx].p_filesz);
		}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
