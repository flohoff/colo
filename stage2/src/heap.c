/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage2/src/heap.c,v 1.2 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

static size_t image_size;
static void *image_base;

static void *free_lo;
static void *free_hi;

static void *next_lo;
static void *next_hi;
static size_t next_size;

void heap_reset(void)
{
	extern char __heap;

	assert(!((unsigned long) &__heap & 15));

	free_lo = &__heap;
	free_hi = KSEG0(ram_size);

	image_size = 0;
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
	printf(image_size ? "%08x %ut\n" : "no image loaded\n", image_size, image_size);
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

int cmnd_heap(int opsz)
{
	if(argc > 1)
		return E_ARGS_OVER;

	if(image_size)
		printf("%08lx - %08lx (%08x %ut)\n",
			(unsigned long) image_base, (unsigned long) image_base + image_size - 1,
			image_size, image_size);
	else
		puts("no image loaded");

	return E_SUCCESS;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
