/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage1/src/fast.c,v 1.3 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

/*
 * these functions are sited in the .data section and
 * will be copied from ROM to RAM (D-cache) by the C
 * startup code (as any other initialised data). this
 * means these functions can be run with the I-cache
 * enabled
 */

/*
 * fast memcpy(), arguments must be word aligned
 */
void *_memcpy_w(void *dst, const void *src, size_t size)
{
	const unsigned *from;
	unsigned count;
	unsigned *to;

	DIE_ON(size & 3);

	size /= 4;
	count = size % 8;
	from = src;
	to = dst;

	switch(count) {

		case 7: to[6] = from[6];
		case 6: to[5] = from[5];
		case 5: to[4] = from[4];
		case 4: to[3] = from[3];
		case 3: to[2] = from[2];
		case 2: to[1] = from[1];
		case 1: to[0] = from[0];
	}
	
	size -= count;
	from += count;
	to += count;

	while(size) {

		to[0] = from[0];
		to[1] = from[1];
		to[2] = from[2];
		to[3] = from[3];
		to[4] = from[4];
		to[5] = from[5];
		to[6] = from[6];
		to[7] = from[7];

		size -= 8;
		from += 8;
		to += 8;
	}

	return dst;
}

/*
 * fast memset(), arguments must be word aligned
 */
void *_memset_w(void *dst, int data, size_t size)
{
	unsigned count;
	unsigned *to;

	DIE_ON(size & 3);

	data &= 0xff;
	data |= data << 8;
	data |= data << 16;

	size /= 4;
	count = size % 8;
	to = dst;

	switch(count) {

		case 7: to[6] = data;
		case 6: to[5] = data;
		case 5: to[4] = data;
		case 4: to[3] = data;
		case 3: to[2] = data;
		case 2: to[1] = data;
		case 1: to[0] = data;
	}
	
	size -= count;
	to += count;

	while(size) {

		to[0] = data;
		to[1] = data;
		to[2] = data;
		to[3] = data;
		to[4] = data;
		to[5] = data;
		to[6] = data;
		to[7] = data;

		size -= 8;
		to += 8;
	}

	return dst;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
