/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

static unsigned cksum;

/*
 * read a number of bytes of ASCII hex, updating checksum
 */
static int gethex(unsigned long *result, unsigned nbyte)
{
	unsigned long value;
	unsigned indx;
	char chr;

	nbyte *= 2;
	value = 0;

	for(indx = 0; indx < nbyte; ++indx) {

		chr = getch();
		if(!isxdigit(chr)) {
			puts("invalid hex in S-record");
			return 0;
		}

		value = (value << 4) | (chr & 0xf);
		if(chr >= 'A')
			value += 9;

		if(indx & 1)
			cksum += value;
	}

	if(result)
		*result = value;

	return 1;
}

long load_srec(void *block, size_t max, long base)
{
	static const int adrsz[] = {
		2, 2, 3, 4, 0, 0, 0, 4, 3, 2
	};
	unsigned long recsz, addr, data;
	unsigned char chr, rec;
	size_t limit;
	int toggle;

	limit = 0;
	toggle = 0;

	do {

		toggle = !toggle;
		putstring(toggle ? " []\b\b\b" : " ][\b\b\b");

		do
			chr = getch();
		while(chr <= ' ');

		if(chr != 's' && chr != 'S') {
			puts("no S-record found");
			return -1;
		}

		rec = getch();
		if(!isdigit(rec)) {
			puts("invalid S-record type");
			return -1;
		}
		rec &= 0xf;

		if(!adrsz[rec]) {
			puts("unknown S-record type");
			return -1;
		}

		cksum = 0;

		if(!gethex(&recsz, 1))
			return -1;

		if(recsz <= adrsz[rec]) {
			puts("invalid S-record length");
			return -1;
		}
		recsz -= adrsz[rec] + 1;

		if(!gethex(&addr, adrsz[rec]))
			return -1;

		if(rec && rec < 4) {

			if(~base) {
				if(addr < base) {
					puts("S-record address out of order");
					return -1;
				}
			} else
				base = addr;

			addr -= base;

			if(addr + recsz > max) {
				puts("S-record data too big");
				return -1;
			}

			for(; recsz; --recsz) {

				if(!gethex(&data, 1))
					break;

				if(rec && rec < 4)
					((uint8_t *) block)[addr++] = data;
			}

			if(addr > limit)
				limit = addr;

		} else

			for(; recsz && gethex(&data, 1); --recsz)
				;

		if(recsz)
			return -1;

		if(!gethex(NULL, 1))
			return -1;

		if(~cksum & 0xff) {
			puts("S-record failed checksum");
			return -1;
		}

	} while(rec < 4);

	return limit;
}

/*
 * load S-records into memory
 */
int cmnd_srec(int opsz)
{
	unsigned long addr, mark;
	size_t size;
	char *ptr;

	if(argc > 2)
		return E_ARGS_OVER;

	addr = ~0;
	if(argc > 1) {
		addr = evaluate(argv[1], &ptr);
		if(*ptr)
			return E_BAD_EXPR;
		if(addr == ~0)
			return E_BAD_VALUE;
	}

	heap_reset();

	size = load_srec(heap_reserve_lo(0), heap_space(), addr);

	if((long) size >= 0) {
		heap_reserve_lo(size);
		heap_alloc();
		heap_info();
	}

	/* wait 'til there's no more junk */

	for(;;) {

		for(mark = MFC0(CP0_COUNT); !kbhit() && MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE;)
			;

		if(!kbhit())
			break;

		getch();
	}

	return (long) size >= 0 ? E_NONE : E_UNSPEC;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
