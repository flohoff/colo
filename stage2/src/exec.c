/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "galileo.h"
#include "cobalt.h"

static void *initrd_reloc;

/*
 * clear initrd relocation
 */
void clear_reloc(void)
{
	initrd_reloc = NULL;
}

/*
 * 'relocate' command
 */
int cmnd_reloc(int opsz)
{
	extern char __executable_start;

	void *image, *initrd, *load;
	size_t imagesz, initrdsz;
	struct elf_info info;
	int elf32;

	if(argc > 1)
		return E_ARGS_OVER;

	initrd = heap_mark_image(&initrdsz);

	if(!initrdsz) {
		puts("no initrd loaded");
		return E_UNSPEC;
	}

	image = heap_image(&imagesz);

	if(!imagesz) {
		puts("no image loaded");
		return E_UNSPEC;
	}

	if(gzip_check(image, imagesz) && !unzip(image, imagesz))
		return E_UNSPEC;

	image = heap_image(&imagesz);

	elf32 = elf32_validate(image, imagesz, &info);

	if(!elf32 && !elf64_validate(image, imagesz, &info)) {
		puts("ELF image invalid");
		return E_UNSPEC;
	}

	load = KSEG0(info.load_phys);

	if(load < KSEG0(0) || load + info.load_size > (void *) &__executable_start) {
		puts("ELF loads outside available memory");
		return E_UNSPEC;
	}

	if(load < image + imagesz && load + info.load_size > image) {
		puts("ELF loads over ELF image");
		return E_UNSPEC;
	}

	if(initrdsz && load < initrd + initrdsz && load + info.load_size > initrd) {
		puts("ELF loads over initrd image");
		return E_UNSPEC;
	}

	initrd_reloc = align_up(load + info.load_size, 4096);

	heap_set_initrd(initrd_reloc, initrdsz);

	return E_NONE;
}

/*
 * launch the loaded image
 */
static int launch_kernel(uint64_t *parm)
{
	extern int launch(void *);
	int (*func)(void *);
	int code;

	parm = KSEG1(parm);
	func = KSEG1(launch);

	icache_flush_all();
	dcache_flush_all();

	code = func(parm);

	icache_flush_all();
	dcache_flush_all();

	/* disable 64-bit addressing */

	MTC0(CP0_STATUS, MFC0(CP0_STATUS) & ~CP0_STATUS_KX);

	printf("\nprogram exited (%d)\n", code);

	heap_reset();

	return code;
}

/*
 * shell command - execute
 */
int cmnd_execute(int opsz)
{
	extern char __executable_start;

	static union
	{
		unsigned long long	d[MAX_CMND_ARGS];
		unsigned long			w[1];
	} args;

	void *image, *load, *initrd;
	size_t imagesz, initrdsz;
	struct elf_info info;
	unsigned long memsz;
	uint64_t parm[6];
	int elf32, indx;

	image = heap_image(&imagesz);

	if(!imagesz) {
		puts("no image loaded");
		return E_UNSPEC;
	}

	if(gzip_check(image, imagesz) && !unzip(image, imagesz))
		return E_UNSPEC;

	initrd = heap_mark_image(&initrdsz);

	image = heap_image(&imagesz);

	elf32 = elf32_validate(image, imagesz, &info);

	if(!elf32 && !elf64_validate(image, imagesz, &info)) {
		puts("ELF image invalid");
		return E_UNSPEC;
	}

	load = KSEG0(info.load_phys);

	if(load < KSEG0(0) || load + info.load_size > (void *) &__executable_start) {
		puts("ELF loads outside available memory");
		return E_UNSPEC;
	}

	if(load < image + imagesz && load + info.load_size > image) {
		puts("ELF loads over ELF image");
		return E_UNSPEC;
	}

	if(initrdsz && load < initrd + initrdsz && load + info.load_size > initrd) {
		puts("ELF loads over initrd image");
		return E_UNSPEC;
	}

	/* relocate initrd */

	if(initrd_reloc) {

		/* this should never happen */

		if(!initrdsz || (unsigned long) initrd_reloc - (unsigned long) load - info.load_size >= 4096) {
			puts("initrd relocation mismatch");
			return E_UNSPEC;
		}

		printf("exec: relocating initrd (%08lx)\n", (unsigned long) initrd_reloc);

		memcpy(initrd_reloc, initrd, initrdsz);
	}

	/* relocate kernel image */

	if(elf32)
		elf32_load(image);
	else
		elf64_load(image);

	if(info.load_size >= 12 && unaligned_load(load + 8) == unaligned_load("CoLo")) {
		puts("refusing to load \"CoLo\" chain loader");
		return E_UNSPEC;
	}

	/* no more network */

	net_down(0);

	/* copy / adjust argument array */

	assert(argc < MAX_CMND_ARGS);

	if(elf32) {

		for(indx = 0; indx < argc; ++indx)
			args.w[indx] = (unsigned long) info.data_sect | (unsigned long) KPHYS(argv[indx]);
		args.w[indx] = 0;

	} else {

		for(indx = 0; indx < argc; ++indx)
			args.d[indx] = info.data_sect | (unsigned long) KPHYS(argv[indx]);
		args.d[indx] = 0;
	}

	/* set up parameter block */

	memsz = (1 << 31) | ram_restrict;
	if(argc > 1)
		memsz |= argc;

	parm[0] = info.data_sect | (unsigned long) KPHYS(&__executable_start);
	parm[1] = info.entry_point;
	parm[2] = memsz;
	parm[3] = info.data_sect | (unsigned long) KPHYS(&args);
	parm[4] = 0;
	parm[5] = 0;

	/* enable 64-bit addressing */

	if(!elf32)
		MTC0(CP0_STATUS, MFC0(CP0_STATUS) | CP0_STATUS_KX);

	/* turn on the light bar on the Qube FIXME */

	*(volatile uint8_t *) BRDG_NCS0_BASE = LED_QUBE_LEFT | LED_QUBE_RIGHT;

	/* go do the thing */

	launch_kernel(parm);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
