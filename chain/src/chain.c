/*
 * (C) P.Horton 2004
 *
 * $Id: loader.c 4 2004-03-28 16:06:07Z pdh $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

typedef unsigned short		__u16;
typedef unsigned				__u32;
typedef unsigned long long	__u64;

typedef short					__s16;
typedef int						__s32;
typedef long long				__s64;

#include "linux/elf.h"

extern void serial_init(void);
extern void putstring(const char *);
extern void puts(const char *);
extern void drain(void);
extern void putchar(int);

#if 0

static void outhex(unsigned long val, unsigned count)
{
	if(count > 1)
		outhex(val >> 4, count - 1);

	val = (val & 0xf) + '0';

	putchar(val > '9' ? val + 7 : val);
}

#endif

static void __attribute__((noreturn)) fatal(void)
{
	for(;;)
		;
}

/*
 * C library function 'memcpy'
 */
void *memcpy(void *dst, const void *src, size_t size)
{
	void *ptr, *end;

	if(!size)
		return dst;

	ptr = dst;
	end = ptr + size;

	while(ptr < end && ((unsigned long) ptr & 3))
		*((uint8_t *) ptr)++ = *((uint8_t *) src)++;

	if(!((unsigned long) src & 3))
		while(ptr < end - 3)
			*((uint32_t *) ptr)++ = *((uint32_t *) src)++;

	while(ptr < end)
		*((uint8_t *) ptr)++ = *((uint8_t *) src)++;

	return dst;
}

/*
 * C library function 'memset'
 */
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

	while(ptr < end && ((unsigned long) ptr & 3))
		*((uint8_t *) ptr)++ = val;

	while(ptr < end - 3)
		*((uint32_t *) ptr)++ = val;

	while(ptr < end)
		*((uint8_t *) ptr)++ = val;

	return dst;
}

/*
 * flush entire D-cache
 */
static void dcache_flush_all(void)
{
	void *line, *end;

	line = KSEG0(0);

	for(end = line + DCACHE_TOTAL_SIZE; line < end; line += DCACHE_LINE_SIZE)
		CACHE(CACHE_IndexWritebackInvD, line);
}

/*
 * load boot loader from ELF image that follows
 */
void chain(void)
{
	extern char __data, __edata;
	extern char __bss, __ebss;
	extern char __stage2;
	extern char __etext;

	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;

	/* initialise .bss / .data */

	memcpy(&__data, &__etext, &__edata - &__data);
	memset(&__bss, 0, &__ebss - &__bss);

	/* */

	serial_init();
	puts("[ CHAIN LOADER ]");
	drain();

	/* */

	eh = (Elf32_Ehdr *) &__stage2;

	if(eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3 ||
		eh->e_ident[EI_CLASS] != ELFCLASS32 ||
		eh->e_ident[EI_DATA] != ELFDATA2LSB ||
		eh->e_ident[EI_VERSION] != EV_CURRENT ||
		eh->e_machine != EM_MIPS ||
		!eh->e_phoff ||
		!eh->e_phnum ||
		eh->e_phentsize != sizeof(Elf32_Phdr))
	{
		puts("[ BAD ELF HEADER ]");
		fatal();
	}

	ph = (void *) eh + eh->e_phoff;

	for(indx = 0; indx < eh->e_phnum; ++indx)

		if(ph[indx].p_type == PT_LOAD) {

			if((ph[indx].p_vaddr & 3) ||
				(ph[indx].p_offset & 3) ||
				(ph[indx].p_filesz & 3) ||
				(ph[indx].p_memsz & 3))
			{
				puts("[ BAD ELF SECTION ]");
				fatal();
			}

			memcpy((void *) ph[indx].p_vaddr, (void *) eh + ph[indx].p_offset, ph[indx].p_filesz);
			memset((void *) ph[indx].p_vaddr + ph[indx].p_filesz, 0, ph[indx].p_memsz - ph[indx].p_filesz);
		}

	/* ensure what we just loaded is in physical memory */

	dcache_flush_all();

	/* jump to it */

	puts("[ LAUNCHING ]");
	drain();

	((void (*)(size_t, size_t, unsigned)) eh->e_entry)(128 << 20, 0, 0); // FIXME
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
