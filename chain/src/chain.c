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

/*
 * fatal error, hang
 */
static void __attribute__((noreturn)) fatal(void)
{
	unsigned leds;

	/*
	 * writing 0x0f to the LED register resets the unit
	 * so we can't turn on all 4 LEDs together
	 *
	 * on the Qube we Flash the light bar and on the RaQ
	 * we flash the "power off" and "web" LEDs alternately
	 */

	for(leds = LED_RAQ_WEB | LED_QUBE_LEFT | LED_QUBE_RIGHT;;) {

		*(volatile uint8_t *) BRDG_NCS0_BASE = leds;
		udelay(400000);

		leds ^= LED_RAQ_WEB | LED_RAQ_POWER_OFF | LED_QUBE_LEFT | LED_QUBE_RIGHT;
	}
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
static void *memset(void *dst, int val, size_t size)
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
 * output value as decimal
 */
static void out_decimal(unsigned long val)
{
	if(val >= 10)
		out_decimal(val / 10);

	putchar(val % 10 + '0');
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
 * read memory bank sizes from Galileo
 */
void memory_info(size_t *bank)
{
	unsigned lo, hi;

	lo = BRDG_REG_WORD(BRDG_REG_RAS01_LOW_DECODE) << 21;
	hi = (BRDG_REG_WORD(BRDG_REG_RAS01_HIGH_DECODE) + 1) << 21;

	bank[0] = hi - lo;
	if(bank[0] > 256 << 20)
		bank[0] = 0;

	lo = BRDG_REG_WORD(BRDG_REG_RAS23_LOW_DECODE) << 21;
	hi = (BRDG_REG_WORD(BRDG_REG_RAS23_HIGH_DECODE) + 1) << 21;

	bank[1] = hi - lo;
	if(bank[1] > 256 << 20)
		bank[1] = 0;
}

/*
 * load boot loader from ELF image that follows
 */
void chain(unsigned arg)
{
	extern char __data, __edata;
	extern char __bss, __ebss;
	extern char __stage2;
	extern char __etext;

	Elf32_Ehdr *eh;
	Elf32_Phdr *ph;
	unsigned indx;
	size_t ram[2];

	/* initialise .bss / .data */

	memcpy(&__data, &__etext, &__edata - &__data);
	memset(&__bss, 0, &__ebss - &__bss);

	/* say hello */

	serial_init();
	puts("chain: running");
	drain();

	/* get memory bank sizes */

	memory_info(ram);

	putstring("chain: bank 0 ");
	out_decimal(ram[0] >> 20);
	putstring("MB\nchain: bank 1 ");
	out_decimal(ram[1] >> 20);
	putchar('\n');

	if(KSEG0(ram[0] + ram[1]) != (void *) arg) {
		puts("chain: MEMORY SIZE MISMATCH");
		fatal();
	}

	/* load stage2 */

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
		puts("chain: BAD ELF HEADER");
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
				puts("chain: BAD ELF SECTION");
				fatal();
			}

			memcpy((void *) ph[indx].p_vaddr, (void *) eh + ph[indx].p_offset, ph[indx].p_filesz);
			memset((void *) ph[indx].p_vaddr + ph[indx].p_filesz, 0, ph[indx].p_memsz - ph[indx].p_filesz);
		}

	/* ensure what we just loaded is in physical memory */

	dcache_flush_all();

	/* jump to it */

	puts("chain: starting stage2");
	drain();

	((void (*)(size_t, size_t, unsigned)) eh->e_entry)(ram[0], ram[1], 0);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
