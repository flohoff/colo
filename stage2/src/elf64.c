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
 * validate ELF64 image
 */
int elf64_validate(const void *image, size_t imagesz, struct elf_info *info)
{
	unsigned indx, phoff, offset, filesz, memsz;
	unsigned long vaddr;
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

	if(eh->e_phoff >> 32)
		return 0;

	phoff = eh->e_phoff;

	if(!eh->e_phnum || eh->e_phentsize != sizeof(Elf64_Phdr))
		return 0;

	if(phoff < sizeof(Elf64_Ehdr) || phoff > imagesz || imagesz - phoff < eh->e_phnum * sizeof(Elf64_Phdr))
		return 0;

	info->region = eh->e_entry;
	info->entry_point = info->region_lo;
	info->region_lo = 0;
	
	/* region must be CKSEG or XKPHYS */

	if(~info->region_hi && (info->region_hi >> 30) != 2) {

		DPRINTF("elf64: invalid region %08x\n", info->region_hi);
		return 0;
	}
			
	ph = (void *) eh + phoff;

	info->load_addr = 0xffffffff;
	info->load_size = 0;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if((ph[indx].p_offset >> 32) ||
				(ph[indx].p_filesz >> 32) ||
				(ph[indx].p_vaddr >> 32) != info->region_hi ||
				(ph[indx].p_memsz >> 32)) {

				return 0;
			}

			offset = ph[indx].p_offset;
			filesz = ph[indx].p_filesz;
			vaddr = ph[indx].p_vaddr;
			memsz = ph[indx].p_memsz;

			if(vaddr + memsz < vaddr)
				return 0;

			if(offset > imagesz || imagesz - offset < filesz)
				return 0;

			if(vaddr < info->load_addr)
				info->load_addr = vaddr;

			if(vaddr + memsz > info->load_addr + info->load_size)
				info->load_size = vaddr + memsz - info->load_addr;
		}

	if(info->load_addr & 3) {

		DPUTS("elf64: load address invalid");
		return 0;
	}

	if((info->entry_point & 3) || info->entry_point < info->load_addr || info->entry_point >= info->load_addr + info->load_size) {

		DPUTS("elf64: entry point invalid");
		return 0;
	}

	DPRINTF("elf64: %08lx - %08lx (%08x:%08lx)\n",
			info->load_addr,
			info->load_addr + info->load_size - 1,
			info->region_hi,
			info->entry_point);

	/* map XKPHYS or CKSEG1 load address to KSEG0 */

	info->load_offset = 0;

	if(~info->region_hi)

		info->load_offset = (long) KSEG0(0);

	else if(info->load_addr >= (unsigned long) KSEG1(0))

		info->load_offset = (long) KSEG0(0) - (long) KSEG1(0);

	else

		return 1;

	DPRINTF("elf64: @%08lx\n", info->load_addr + info->load_offset);

	return 1;
}

/*
 * load ELF64 image
 */
void elf64_load(const void *image, long offset)
{
	unsigned indx, phoff, fileofs, filesz, memsz;
	unsigned long vaddr, entry;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;

	eh = (Elf64_Ehdr *) image;

	phoff = eh->e_phoff;
	entry = eh->e_entry;

	ph = (void *) eh + phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			vaddr = ph[indx].p_vaddr;
			fileofs = ph[indx].p_offset;
			filesz = ph[indx].p_filesz;
			memsz = ph[indx].p_memsz;

			vaddr += offset;

			DPRINTF("elf64: %08lx (%08lx) <-- %08x %ut + %ut\n",
				vaddr - offset,
				vaddr,
				fileofs,
				filesz,
				memsz - filesz);

			memcpy((void *) vaddr, (void *) eh + fileofs, filesz);

			memset((void *) vaddr + filesz, 0, memsz - filesz);
		}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
