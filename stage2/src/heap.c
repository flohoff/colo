/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

static size_t image_size;
static void *image_base;

static size_t image_size_mark;
static void *image_base_mark;

static void *free_lo;
static void *free_hi;

static void *next_lo;
static void *next_hi;
static size_t next_size;

void heap_reset(void)
{
	extern char __text;

	assert(!((unsigned long) &__text & 15));

	free_lo = KSEG0(0);
	free_hi = KSEG0(&__text) - (32 << 10); // XXX

	image_size = 0;
	image_size_mark = 0;

	env_remove_tag(VAR_INITRD);
}

void heap_initrd_vars(void)
{
	char text[16];

	if(image_size_mark) {

		sprintf(text, "%lx", (unsigned long) image_base_mark);
		env_put("initrd-start", text, VAR_INITRD);
		sprintf(text, "%lx", (unsigned long) image_base_mark + image_size_mark);
		env_put("initrd-end", text, VAR_INITRD);
		sprintf(text, "%x", image_size_mark);
		env_put("initrd-size", text, VAR_INITRD);
	}
}

size_t heap_space(void)
{
	return free_hi - free_lo;
}

void *heap_reserve_lo(size_t size)
{
	next_size = size;

	size = (size + 15) & ~15;

	if(size > heap_space())
		return NULL;

	next_lo = free_lo + size;
	next_hi = free_hi;

	return free_lo;
}

void *heap_reserve_hi(size_t size)
{
	next_size = size;

	size = (size + 15) & ~15;

	if(size > heap_space())
		return NULL;

	next_lo = free_lo;
	next_hi = free_hi - size;

	return next_hi;
}

void heap_info(void)
{
	if(image_size) {
		printf("%08x %ut\n", image_size, image_size);
		if(image_size_mark)
			printf("%08x %ut\n", image_size_mark, image_size_mark);
	} else
		puts("no image loaded");
}

void heap_alloc(void)
{
	if(next_lo != free_lo) {

		image_base = free_lo;
		free_lo = next_lo;

	} else {

		image_base = next_hi;
		free_hi = next_hi;
	}

	image_size = next_size;
}

void *heap_image(size_t *size)
{
	if(size)
		*size = image_size;

	return image_base;
}

void heap_mark(void)
{
	image_base_mark = image_base;
	image_size_mark = image_size;
}

void *heap_mark_image(size_t *size)
{
	if(size)
		*size = image_size_mark;

	return image_base_mark;
}

int cmnd_heap(int opsz)
{
	if(argc > 1)
		return E_ARGS_OVER;

	if(image_size) {
		printf("%08lx - %08lx (%08x %ut)\n",
			(unsigned long) image_base, (unsigned long) image_base + image_size - 1,
			image_size, image_size);
		if(image_size_mark)
			printf("%08lx - %08lx (%08x %ut)\n",
				(unsigned long) image_base_mark, (unsigned long) image_base_mark + image_size_mark - 1,
				image_size_mark, image_size_mark);
	} else
		printf("no image loaded (%uKB)\n", heap_space() >> 10);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
