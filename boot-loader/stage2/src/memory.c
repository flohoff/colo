/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "md5.h"

extern char __heap;

/*
 * write byte/half/word to memory (possibly unaligned)
 */
static void mem_poke(void *addr, unsigned data, int size)
{
	union {
		volatile uint8_t	*b;
		volatile uint16_t	*h;
		volatile uint32_t	*w;
	} p;

	p.w = addr;

	switch(size)
	{
		case 1:
			p.b[0] = data;
			break;

		case 2:
			if((unsigned long) addr & 1) {
				p.b[0] = data;
				p.b[1] = data >> 8;
			} else
				p.h[0] = data;
			break;

		default:
			if((unsigned long) addr & 3)
				unaligned_store((void *) p.w, data);
			else
				p.w[0] = data;
	}
}

/*
 * read byte/half/word from memory (possibly unaligned)
 */
static unsigned mem_peek(const void *addr, int size)
{
	union {
		volatile uint8_t	*b;
		volatile uint16_t	*h;
		volatile uint32_t	*w;
	} p;

	p.w = (void *) addr;

	switch(size)
	{
		case 1:
			return p.b[0];

		case 2:
			if((unsigned long) addr & 1)
				return p.b[0] | ((unsigned) p.b[1] << 8);
			return p.h[0];

		default:
			if((unsigned long) addr & 3)
				return unaligned_load((void *) p.w);
			return p.w[0];
	}
}

/*
 * return correct pointer for accessing range of addresses
 *
 * if returning a pointer to uncached space that overlaps
 * RAM we writeback the RAM contents
 */
static void *kseg_addr(unsigned long addr, size_t size)
{
	addr = (unsigned long) KPHYS(addr);

	if(addr + size <= ram_size)
		return KSEG0(addr);

	if(addr < ram_size)
		dcache_flush(addr, ram_size - addr);

	return KSEG1(addr);
}

/*
 * shell command - read memory
 */
int cmnd_read(int opsz)
{
	static struct {
		unsigned long	addr;
		int				size;
	} last = {
		.addr = (unsigned long) &__heap,
		.size = 4,
	};

	unsigned long addr;
	char *ptr;

	if(argc > 2)
		return E_ARGS_OVER;

	if(argc > 1) {
		addr = evaluate(argv[1], &ptr);
		if(*ptr)
			return E_BAD_EXPR;
		last.addr = addr;
	}
	
	if(opsz)
		last.size = opsz;

	printf("%08lx %0*x\n", last.addr, last.size * 2, mem_peek(kseg_addr(last.addr, last.size), last.size));

	last.addr += last.size;

	return E_SUCCESS;
}

/*
 * shell command - write memory
 */
int cmnd_write(int opsz)
{
	static struct {
		unsigned long	addr;
		int				size;
	} last = {
		.addr = (unsigned long) &__heap,
		.size = 4,
	};

	unsigned size, data;
	unsigned long addr;
	char *ptr;

	if(argc < 2)
		return E_ARGS_UNDER;

	if(argc > 3)
		return E_ARGS_OVER;

	size = opsz ? opsz : last.size;

	data = evaluate(argv[argc - 1], &ptr);
	if(*ptr)
		return E_BAD_EXPR;

	if(size < 4 && (data & (~0 << (size * 8))))
		return E_BAD_VALUE;

	if(argc > 2) {
		addr = evaluate(argv[1], &ptr);
		if(*ptr)
			return E_BAD_EXPR;
		last.addr = addr;
	}
	
	last.size = size;

	mem_poke(kseg_addr(last.addr, last.size), data, last.size);

	printf("%08lx %0*x\n", last.addr, last.size * 2, data);

	last.addr += last.size;

	return E_SUCCESS;
}

/*
 * shell command - dump memory
 */
int cmnd_dump(int opsz)
{
	static struct {
		unsigned long	addr;
		unsigned			count;
		int				size;
	} last = {
		.addr = (unsigned long) &__heap,
		.count = 0x100,
		.size = 4
	};

	unsigned size, count, indx, data;
	unsigned long addr;
	char text[16];
	void *base;
	char *ptr;

	if(argc > 3)
		return E_ARGS_OVER;

	size = opsz ? opsz : last.size;
	count = last.count;

	if(argc > 2) {
		count = evaluate(argv[2], &ptr);
		if(*ptr)
			return E_BAD_EXPR;
		count *= size;
	}

	if(argc > 1 && strcmp(argv[1], ".")) {
		addr = evaluate(argv[1], &ptr);
		if(*ptr)
			return E_BAD_EXPR;
		last.addr = addr;
	}

	if(count < 0x1000)
		last.count = count;
	last.size = size;

	for(base = kseg_addr(last.addr, count); count;) {

		printf("%08lx ", last.addr);

		for(indx = 0; indx < 16; indx += last.size) {

			if(!(indx & 7))
				putchar(' ');

			if(indx < count) {
				data = mem_peek(base + indx, last.size);
				printf("%0*x ", last.size * 2, data);
				mem_poke(text + indx, data, last.size);
			} else
				printf("%.*s ", last.size * 2, "--------");
		}

		for(indx = 0; indx < 16 && indx < count; ++indx) {

			if(!(indx & 7))
				putchar(' ');

			text[indx] &= 0x7f;
			putchar(isprint(text[indx]) ? text[indx] : '.');
		}

		putchar('\n');

		if(kbhit()) {
			getch();
			break;
		}

		count -= indx;
		last.addr += indx;
		base += indx;
	}

	return E_SUCCESS;
}

int cmnd_md5sum(int opsz)
{
	struct MD5Context ctx;
	uint8_t digest[16];
	unsigned long addr;
	unsigned indx;
	size_t size;
	char *ptr;

	if(argc > 1) {

		if(argc == 2)
			return E_ARGS_COUNT;
		if(argc > 3)
			return E_ARGS_OVER;

		addr = evaluate(argv[1], &ptr);
		if(*ptr)
			return E_BAD_EXPR;

		size = evaluate(argv[2], &ptr);
		if(*ptr)
			return E_BAD_EXPR;

	} else {

		addr = (unsigned long) heap_image(&size);

		if(!size) {
			puts("no data loaded");
			return E_SUCCESS;
		}
	}

	MD5Init(&ctx);
	MD5Update(&ctx, kseg_addr(addr, size), size);
	MD5Final(digest, &ctx);

	for(indx = 0; indx < sizeof(digest); ++indx)
		printf("%02x", digest[indx]);
	putchar('\n');

	return E_SUCCESS;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
