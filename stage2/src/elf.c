/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "galileo.h"
#include "linux/elf.h"
#include "cobalt.h"

extern void *initrd_image(size_t *);

/*
 * Check for valid ELF image and return base address and size, 32 bit version
 */
static void *elf_check_32(const void *image, size_t imagesz, size_t *size)
{
	unsigned long floor, ceiling;
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;

	if(imagesz < sizeof(Elf32_Ehdr))
		return NULL;

	eh = (Elf32_Ehdr *) image;

	if(eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3 ||
		eh->e_ident[EI_CLASS] != ELFCLASS32 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_ident[EI_VERSION] != EV_CURRENT ||
		eh->e_machine != EM_MIPS) {

		return NULL;
	}

	if(!eh->e_phoff || !eh->e_phnum || eh->e_phentsize != sizeof(Elf32_Phdr))
		return NULL;

	if(eh->e_phoff > imagesz || imagesz - eh->e_phoff < eh->e_phnum * sizeof(Elf32_Phdr))
		return NULL;

	ph = (void *) eh + eh->e_phoff;

	floor = -1;
	ceiling = 0;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if(ph[indx].p_offset > imagesz || imagesz - ph[indx].p_offset < ph[indx].p_filesz)
				return NULL;

			if(ph[indx].p_vaddr < floor)
				floor = ph[indx].p_vaddr;

			if(ph[indx].p_vaddr + ph[indx].p_memsz > ceiling)
				ceiling = ph[indx].p_vaddr + ph[indx].p_memsz;
		}

	if(floor > ceiling)
		return NULL;

	if(eh->e_entry < floor || eh->e_entry >= ceiling)
		return NULL;

	if(size)
		*size = ceiling - floor;

	return KSEG0(floor);
}

/*
 * load ELF image, 32 bit version
 */
static void *elf_load_32(const void *image, size_t size)
{
	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;

	if(!elf_check_32(image, size, NULL))
		return NULL;

	eh = (Elf32_Ehdr *) image;

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			DPRINTF("elf: %08x <-- %08x %ut + %ut\n",
				ph[indx].p_vaddr,
				ph[indx].p_offset,
				ph[indx].p_filesz,
				ph[indx].p_memsz - ph[indx].p_filesz);

			memcpy((void *) ph[indx].p_vaddr, (void *) eh + ph[indx].p_offset, ph[indx].p_filesz);

			memset((void *) ph[indx].p_vaddr + ph[indx].p_filesz, 0, ph[indx].p_memsz - ph[indx].p_filesz);
		}

	DPRINTF("elf: entry %08x\n", eh->e_entry);

	return KSEG0(eh->e_entry);
}

/*
 * Check for valid ELF image and return base address and size, 64 bit version
 */
static void *elf_check_64(const void *image, size_t imagesz, size_t *size)
{
	unsigned indx, phoff, offset, filesz, memsz;
	unsigned long floor, ceiling, entry, vaddr;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;

	if(imagesz < sizeof(Elf64_Ehdr))
		return NULL;

	eh = (Elf64_Ehdr *) image;

	if(eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3 ||
		eh->e_ident[EI_CLASS] != ELFCLASS64 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_ident[EI_VERSION] != EV_CURRENT ||
		eh->e_machine != EM_MIPS) {

		return NULL;
	}

	if((eh->e_phoff >> 32) ||
		(eh->e_entry >> 32)) {

		return NULL;
	}

	phoff = eh->e_phoff;
	entry = eh->e_entry;

	if(!phoff || !eh->e_phnum || eh->e_phentsize != sizeof(Elf64_Phdr))
		return NULL;

	if(phoff > imagesz || imagesz - phoff < eh->e_phnum * sizeof(Elf64_Phdr))
		return NULL;

	ph = (void *) eh + phoff;

	floor = -1;
	ceiling = 0;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if((ph[indx].p_offset >> 32) ||
				(ph[indx].p_filesz >> 32) ||
				(ph[indx].p_vaddr >> 32) ||
				(ph[indx].p_memsz >> 32)) {

				return NULL;
			}

			offset = ph[indx].p_offset;
			filesz = ph[indx].p_filesz;
			vaddr = ph[indx].p_vaddr;
			memsz = ph[indx].p_memsz;

			if(offset > imagesz || imagesz - offset < filesz)
				return NULL;

			if(vaddr < floor)
				floor = vaddr;

			if(vaddr + memsz > ceiling)
				ceiling = vaddr + memsz;
		}

	if(floor > ceiling)
		return NULL;

	if(entry < floor || entry >= ceiling)
		return NULL;

	if(size)
		*size = ceiling - floor;

	return KSEG0(floor);
}

/*
 * load ELF image, 64 bit version
 */
static void *elf_load_64(const void *image, size_t size)
{
	unsigned indx, phoff, offset, filesz, memsz;
	unsigned long vaddr, entry;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;

	if(!elf_check_64(image, size, NULL))
		return NULL;

	eh = (Elf64_Ehdr *) image;

	phoff = eh->e_phoff;
	entry = eh->e_entry;

	ph = (void *) eh + phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			vaddr = ph[indx].p_vaddr;
			offset = ph[indx].p_offset;
			filesz = ph[indx].p_filesz;
			memsz = ph[indx].p_memsz;

			DPRINTF("elf64: %08lx <-- %08x %ut + %ut\n",
				vaddr,
				offset,
				filesz,
				memsz - filesz);

			memcpy((void *) vaddr, (void *) eh + offset, filesz);

			memset((void *) vaddr + filesz, 0, memsz - filesz);
		}

	DPRINTF("elf64: entry %08lx\n", entry);

	return KSEG0(entry);
}

/*
 * Check for valid ELF image and return base address and size
 */
void *elf_check(const void *image, size_t imagesz, size_t *size)
{
	void *ptr;

	ptr = elf_check_32(image, imagesz, size);

	return ptr ? ptr : elf_check_64(image, imagesz, size);
}

/*
 * Load ELF image
 */
void *elf_load(const void *image, size_t size)
{
	void *ptr;

	ptr = elf_load_32(image, size);

	return ptr ? ptr : elf_load_64(image, size);
}

int cmnd_execute(int opsz)
{
	extern unsigned launch(long long, long long, int, int, int, int);
	extern char __text;

	void *image, *targ, *func, *initrd;
	size_t imagesz, elfsz, initrdsz;
	unsigned code;

	image = heap_image(&imagesz);

	if(!imagesz) {
		puts("no image loaded");
		return E_UNSPEC;
	}

	if(gzip_check(image, imagesz) && !unzip(image, imagesz))
		return E_UNSPEC;

	initrd = heap_mark_image(&initrdsz);

	image = heap_image(&imagesz);

	targ = elf_check(image, imagesz, &elfsz);

	if(!targ) {
		puts("ELF image invalid");
		return E_UNSPEC;
	}

	if(targ < KSEG0(0) || targ + elfsz > (void *) &__text) {
		puts("ELF loads outside available memory");
		return E_UNSPEC;
	}

	if(targ < image + imagesz && targ + elfsz > image) {
		puts("ELF loads over ELF image");
		return E_UNSPEC;
	}

	if(initrdsz && targ < initrd + initrdsz && targ + elfsz > initrd) {
		puts("ELF loads over initrd image");
		return E_UNSPEC;
	}

	func = elf_load(image, imagesz);
	if(!func)
		return E_UNSPEC;

	if(elfsz >= 12 && unaligned_load(targ + 8) == unaligned_load("CoLo")) {
		puts("Refusing to load \"CoLo\" chain loader");
		return E_UNSPEC;
	}

	net_down(0);

	/* turn on the light bar on the Qube FIXME */

	*(volatile uint8_t *) BRDG_NCS0_BASE = LED_QUBE_LEFT | LED_QUBE_RIGHT;

	/* ensure caches are consistent */

	icache_flush_all();
	dcache_flush_all();

	/* relocate stack to top of RAM and call target */

	code = launch(
			(int) &__text,			/* sign extend to 64-bits */
			(int) func,				/* sign extend to 64-bits */
			(int) KSEG0(ram_restrict) | (argc > 1 ? argc : 0),
			(int) argv,
			0,
			0);

	printf("exited #%08x\n", code);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
