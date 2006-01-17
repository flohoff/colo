/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <elf.h>
#include "../../include/rfx.h"

#define VER_MAJOR					0
#define VER_MINOR					1

#define MAX_RELOCS				50000

#define APP_NAME					"elf2rfx"

#define __STR(x)					#x
#define _STR(x)					__STR(x)

#define NO_ERRNO					0
#define INV_LOAD_ADDR			1

static unsigned va_base;
static unsigned va_size;
static unsigned va_memsz;
static Elf32_Ehdr *eh;
static Elf32_Shdr *sh;
static int verbose;
static unsigned *relocs;
static unsigned nrelocs;

/*
 * print error message and exit
 */
static void __attribute__((noreturn)) fatal(int err, const char *fmt, ...)
{
	static char msg[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	msg[sizeof(msg) - 1] = '\0';

	if(err)
		fprintf(stderr, APP_NAME ": %s (%s)\n", msg, strerror(err));
	else
		fprintf(stderr, APP_NAME ": %s\n", msg);

	exit(1);
}

/*
 * generate binary output file
 */
static void output_bin(int fd, unsigned va)
{
	unsigned indx, type, data;
	unsigned *pfix;
	void *bin;

	if(ftruncate(fd, va_size))
		fatal(errno, "failed to set file size");

	bin = mmap(0, va_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(bin == MAP_FAILED)
		fatal(errno, "failed to map output file");

	for(indx = 0; indx < eh->e_shnum; ++indx)
		if(sh[indx].sh_type == SHT_PROGBITS && (sh[indx].sh_flags & SHF_ALLOC))
			memcpy(bin + sh[indx].sh_addr - va_base, (void *) eh + sh[indx].sh_offset, sh[indx].sh_size);

	for(indx = 0; indx < nrelocs; ++indx) {

		type = relocs[indx] & 3;
		pfix = bin + (relocs[indx] & ~3);
		data = *pfix;

		switch(type) {

			case RFX_REL_32:
				*pfix = data + va;
				break;

			case RFX_REL_26:
				data += (va & 0x0fffffff) >> 2;
				if((*pfix ^ data) & 0xfc000000)
					fatal(NO_ERRNO, "RFX_REL_26 relocation out of range");
				*pfix = data;
				break;

			case RFX_REL_H16:
				*pfix = (data & 0xffff0000) | ((data + (va >> 16)) & 0x0000ffff);
				break;

			default:
				fatal(NO_ERRNO, "unexpected RFX relocation");
		}
	}

	munmap(bin, va_size);
}

/*
 * generate RFX output file
 */
static void output_rfx(int fd)
{
	struct rfx_header *rfx;
	unsigned indx;
	off_t size;
	void *base;

	size = sizeof(struct rfx_header) + va_size + sizeof(unsigned) * nrelocs;

	if(ftruncate(fd, size))
		fatal(errno, "failed to set file size");

	rfx = mmap(0, size, PROT_WRITE, MAP_SHARED, fd, 0);
	if(rfx == MAP_FAILED)
		fatal(errno, "failed to map output file");

	memcpy(rfx->magic, RFX_HDR_MAGIC, RFX_HDR_MAGIC_SZ);
	rfx->imgsize = va_size;
	rfx->memsize = va_memsz;
	rfx->entry = eh->e_entry - va_base;
	rfx->nrelocs = nrelocs;

	base = rfx + 1;

	for(indx = 0; indx < eh->e_shnum; ++indx)
		if(sh[indx].sh_type == SHT_PROGBITS && (sh[indx].sh_flags & SHF_ALLOC))
			memcpy(base + sh[indx].sh_addr - va_base, (void *) eh + sh[indx].sh_offset, sh[indx].sh_size);

	memcpy(base + va_size, relocs, sizeof(unsigned) * nrelocs);

	munmap(rfx, size);
}

/*
 * map ELF file into memory and validate it
 */
static void load_elf(const char *name)
{
	unsigned indx, top;
	off_t elfsz;
	int fd;

	fd = open(name, O_RDONLY);
	if(fd == -1)
		fatal(errno, "failed to open \"%s\"", name);

	elfsz = lseek(fd, 0, SEEK_END);
	if(elfsz == (off_t) -1)
		fatal(errno, "seek failed on input file");

	eh = mmap(0, elfsz, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if(eh == MAP_FAILED)
		fatal(errno, "failed to map input file");

	close(fd);

	if(elfsz < sizeof(Elf32_Ehdr) ||
		memcmp(&eh->e_ident[EI_MAG0], ELFMAG, SELFMAG) ||
		eh->e_machine != EM_MIPS ||
		eh->e_ident[EI_CLASS] != ELFCLASS32 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_type != ET_EXEC) {

		fatal(NO_ERRNO, "input file is unrecognised");
	}

	if(!eh->e_shoff ||
		!eh->e_shnum ||
		eh->e_shentsize != sizeof(Elf32_Shdr) ||
		eh->e_shoff + eh->e_shnum * sizeof(Elf32_Shdr) > elfsz) {

		fatal(NO_ERRNO, "unable to find section headers");
	}

	sh = (void *) eh + eh->e_shoff;

	va_base = -1;
	va_size = 0;

	for(indx = 0; indx < eh->e_shnum; ++indx)
		if(sh[indx].sh_type == SHT_PROGBITS && (sh[indx].sh_flags & SHF_ALLOC)) {
			if(sh[indx].sh_offset + sh[indx].sh_size > elfsz)
				fatal(NO_ERRNO, "invalid section header #%u", indx);
			if(sh[indx].sh_addr < va_base)
				va_base = sh[indx].sh_addr;
			top = sh[indx].sh_addr + sh[indx].sh_size - va_base;
			if(top > va_size)
				va_size = top;
		}

	va_memsz = va_size;

	for(indx = 0; indx < eh->e_shnum; ++indx)
		if(sh[indx].sh_type == SHT_NOBITS && (sh[indx].sh_flags & SHF_ALLOC)) {
			if(sh[indx].sh_addr < va_base + va_size)
				fatal(NO_ERRNO, "SHT_NOBITS section below SHT_PROGBITS");
			top = sh[indx].sh_addr + sh[indx].sh_size - va_base;
			if(top > va_memsz)
				va_memsz = top;
		}

	if((va_base | va_size) & 3)
		fatal(NO_ERRNO, "program misaligned");

	if((va_base ^ (va_base + va_size)) & 0xf0000000)	/* check we can relocate R_MIPS_26 */
		fatal(NO_ERRNO, "program spans 256MB page");

	if(eh->e_entry < va_base || eh->e_entry >= va_base + va_size)
		fatal(NO_ERRNO, "program entry point out of range");

	if(verbose) {
		printf("load address 0x%08x\n", va_base);
		printf("load size    0x%08x (%u)\n", va_size, va_size);
		printf("memory size  0x%08x (%u)\n", va_memsz, va_memsz);
		printf("entry point  0x%08x\n", eh->e_entry);
	}
}

/*
 * perform single ELF relocation
 */
static void reloc_single(Elf32_Shdr *sect, Elf32_Rel *rel)
{
	unsigned type, data, rfxr;
	unsigned *pfix;

	if((rel->r_offset & 3) ||
		rel->r_offset < sect->sh_addr ||
		rel->r_offset >= sect->sh_addr + sect->sh_size) {

		fatal(NO_ERRNO, "invalid relocation");
	}

	type = ELF32_R_TYPE(rel->r_info);
	pfix = (void *) eh + sect->sh_offset + rel->r_offset - sect->sh_addr;
	data = *pfix;

	switch(type) {

		case R_MIPS_32:
			*pfix = data - va_base;
			rfxr = RFX_REL_32;
			break;

		case R_MIPS_26:
			data -= (va_base & 0x0fffffff) >> 2;
			if((*pfix ^ data) & 0xfc000000)
				fatal(NO_ERRNO, "R_MIPS_26 relocation out of range");
			*pfix = data;
			rfxr = RFX_REL_26;
			break;

		case R_MIPS_HI16:
		case R_MIPS_LO16:
			if(va_base & 0xffff)
				fatal(NO_ERRNO, "program requires full R_MIPS_HI16/R_MIPS_LO16 relocation");
			if(type != R_MIPS_HI16)
				return;
			*pfix = (data & 0xffff0000) | ((data - (va_base >> 16)) & 0x0000ffff);
			rfxr = RFX_REL_H16;
			break;

		case R_MIPS_GPREL16:
			return;

		default:
			fatal(NO_ERRNO, "unknown relocation type #%u", type);
	}

	if(nrelocs >= MAX_RELOCS)
		fatal(NO_ERRNO, "too many relocations");
	relocs[nrelocs++] = (rel->r_offset - va_base) | rfxr;
}

/*
 * relocate ELF image to VA 0x00000000
 */
static void reloc_elf(void)
{
	unsigned indx, count;
	Elf32_Shdr *targ;
	Elf32_Rel *rel;

	relocs = mmap(0, sizeof(unsigned) * MAX_RELOCS, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if(relocs == MAP_FAILED)
		fatal(errno, "failed to map memory for relocation table");

	for(indx = 0; indx < eh->e_shnum; ++indx)
		if(sh[indx].sh_size && (sh[indx].sh_type == SHT_REL || sh[indx].sh_type == SHT_RELA)) {

			if(sh[indx].sh_info >= eh->e_shnum)
				fatal(NO_ERRNO, "relocation for missing section #%u", indx);
			targ = &sh[sh[indx].sh_info];

			if(targ->sh_type == SHT_PROGBITS && (targ->sh_flags & SHF_ALLOC)) {

				if(sh[indx].sh_type == SHT_RELA)
					fatal(NO_ERRNO, "no support for section SHT_RELA");

				if(sh[indx].sh_offset & 3)
					fatal(NO_ERRNO, "misaligned section #%u", indx);

				if((targ->sh_offset & 3) || (targ->sh_addr & 3))
					fatal(NO_ERRNO, "misaligned section #%u", sh[indx].sh_info);

				rel = (void *) eh + sh[indx].sh_offset;

				count = sh[indx].sh_size / sizeof(Elf32_Rel);
				if(verbose)
					printf("relocating   #%u (%u)\n", sh[indx].sh_info, count);

				while(count--)
					reloc_single(targ, rel++);
			}
		}

	if(verbose)
		printf("relocations  %u\n", nrelocs);
}

/*
 * print usage message and exit
 */
static void usage(void)
{
	puts("\nusage: " APP_NAME " [ -v ] [ -f ] [ -b address ] in-file out-file\n");
	puts("  v" _STR(VER_MAJOR) "." _STR(VER_MINOR) " (" __DATE__ ")\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int opt, over, fd;
	unsigned va;
	char *ptr;

	va = INV_LOAD_ADDR;
	over = 0;

	opterr = 0;

	while((opt = getopt(argc, argv, "fvb:")) != -1)
		switch(opt) {

			case 'f':
				over = 1;
				break;

			case 'v':
				++verbose;
				break;
				
			case 'b':
				va = strtoul(optarg, &ptr, 16);
				if(*ptr || (va & 3))
					fatal(NO_ERRNO, "invalid load address");
				break;

			default:
				usage();
		}

	if(argc - optind != 2)
		usage();

	load_elf(argv[optind]);
	reloc_elf();

	fd = open(argv[optind + 1], O_CREAT | O_TRUNC | O_RDWR | (over ? 0 : O_EXCL), 0664);
	if(fd == -1)
		fatal(errno, "failed to create \"%s\"", argv[optind + 1]);

	if(va != INV_LOAD_ADDR)
		output_bin(fd, va);
	else
		output_rfx(fd);

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
