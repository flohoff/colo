/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

void *memcpy(void *dst, const void *src, size_t size)
{
	void *ptr, *end;

	if(!size)
		return dst;

	ptr = dst;
	end = ptr + size;

	while(ptr < end && ((unsigned long) ptr & 3)) {
		*(uint8_t *) ptr = *(uint8_t *) src;
		++ptr, ++src;
	}

	if(!((unsigned long) src & 3))
		while(ptr < end - 3) {
			*(uint32_t *) ptr = *(uint32_t *) src;
			ptr += 4, src += 4;
		}

	while(ptr < end) {
		*(uint8_t *) ptr = *(uint8_t *) src;
		++ptr, ++src;
	}

	return dst;
}

void *memset(void *dst, int val, size_t size)
{
	void *ptr, *end;

	if(!size)
		return dst;

	val &= 0xff;
	val |= val << 8;
	val |= val << 16;

	ptr = dst;
	end = ptr + size;

	while(ptr < end && ((unsigned long) ptr & 3)) {
		*(uint8_t *) ptr = val;
		++ptr;
	}

	while(ptr < end - 3) {
		*(uint32_t *) ptr = val;
		ptr += 4;
	}

	while(ptr < end) {
		*(uint8_t *) ptr = val;
		++ptr;
	}

	return dst;
}

int memcmp(const void *mem1, const void *mem2, size_t size)
{
	unsigned dat1, dat2;

	if(!size)
		return 0;

	do {

		dat1 = *(unsigned char *) mem1;
		dat2 = *(unsigned char *) mem2;
		++mem1, ++mem2;

	} while(--size && dat1 == dat2);

	return (int) dat1 - (int) dat2;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
