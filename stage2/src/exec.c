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

int cmnd_execute(int opsz)
{
	extern unsigned launch(uint64_t, uint64_t, uint64_t, uint64_t);
	extern char __text;

	void *image, *load, *initrd;
	size_t imagesz, initrdsz;
	struct elf_info info;
	unsigned code;
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

	if(!elf32 && !elf64_validate(image, imagesz, &info)) {
		puts("ELF image invalid");
		return E_UNSPEC;
	}

	load = (void *) info.load_addr + info.load_offset;

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
		elf32_load(image, info.load_offset);
	else
		elf64_load(image, info.load_offset);

	if(info.load_size >= 12 && unaligned_load(load + 8) == unaligned_load("CoLo")) {
		puts("Refusing to load \"CoLo\" chain loader");
		return E_UNSPEC;
	}

	net_down(0);

	/* turn on the light bar on the Qube FIXME */

	*(volatile uint8_t *) BRDG_NCS0_BASE = LED_QUBE_LEFT | LED_QUBE_RIGHT;

	/* ensure caches are consistent */

	icache_flush_all();
	dcache_flush_all();

	/* enable 64-bit addressing */

	if(!elf32)
		MTC0(CP0_STATUS, MFC0(CP0_STATUS) | CP0_STATUS_KX);

	/* relocate stack to top of RAM and call target */

	code = launch(
			KSEG_TO_CKSEG(&__text),
			info.r.region | info.entry_point,
			KSEG_TO_CKSEG((unsigned long) KSEG0(ram_restrict) | (argc > 1 ? argc : 0)),
			KSEG_TO_CKSEG(argv));

	printf("exited #%08x\n", code);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
