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
#include "cobalt.h"

#define KSEG_TO_CKSEG(p)			((long long)(long)(p))

extern void *initrd_image(size_t *);

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

	printf("\nExited (%d)\n", code);

	return code;
}

int cmnd_execute(int opsz)
{
	extern char __text;

	void *image, *load, *initrd, *top;
	size_t imagesz, initrdsz;
	struct elf_info info;
	uint64_t parm[6];
	int elf32;

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

	if(!elf32 /*&& !elf64_validate(image, imagesz, &info)*/) {
		puts("ELF image invalid");
		return E_UNSPEC;
	}

	load = KSEG0(info.load_phys);

	if(load < KSEG0(0) || load + info.load_size > (void *) &__text) {
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

	if(elf32)
		elf32_load(image);
	/*
	else
		elf64_load(image);
	*/

	if(info.load_size >= 12 && unaligned_load(load + 8) == unaligned_load("CoLo")) {
		puts("Refusing to load \"CoLo\" chain loader");
		return E_UNSPEC;
	}

	/* no more network */

	net_down(0);

	/* set up parameter block */

	top = KSEG0(ram_restrict);
	if(argc > 1)
		top += argc;

	parm[0] = KSEG_TO_CKSEG(&__text);
	parm[1] = info.entry_point;
	parm[2] = KSEG_TO_CKSEG(top);
	parm[3] = KSEG_TO_CKSEG(argv);
	parm[4] = 0;
	parm[5] = 0;

#ifdef _DEBUG
	{
		int indx;

		for(indx = 0; indx < 6; ++indx)
			printf("%d: %08x.%08x\n", indx, double_word_hi(parm[indx]), double_word_lo(parm[indx]));
	}
#endif

	/* turn on the light bar on the Qube FIXME */

	*(volatile uint8_t *) BRDG_NCS0_BASE = LED_QUBE_LEFT | LED_QUBE_RIGHT;

	/* enable 64-bit addressing */

	if(!elf32)
		MTC0(CP0_STATUS, MFC0(CP0_STATUS) | CP0_STATUS_KX);

	/* relocate stack to top of RAM and call target */

	launch_kernel(parm);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
