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
#include "rfx.h"
#include "version.h"

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
 * load boot loader from RFX image that follows
 */
void chain(unsigned arg)
{
	extern char __stage2;

	unsigned indx, data, type, switches;
	unsigned *pfix, *relocs;
	unsigned long loadaddr;
	struct rfx_header *rfx;
	size_t ram[2];

	/* say hello */

	serial_init();
	puts("chain: v" _STR(VER_MAJOR) "." _STR(VER_MINOR) " (" __DATE__ ")");
	drain();

	/* read state of buttons */

	switches = *(volatile unsigned *) BRDG_NCS2_BASE >> 24;

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

	rfx = (void *) &__stage2;
	
	if(memcmp(rfx->magic, RFX_HDR_MAGIC, RFX_HDR_MAGIC_SZ)) {
		puts("chain: RFX header missing");
		fatal();
	}

	loadaddr = ram[0] + ram[1] - (32 << 10); // XXX

	if(rfx->memsize > loadaddr) {
		puts("chain: out of memory");
		fatal();
	}

	loadaddr = (unsigned long) KSEG0((loadaddr - rfx->memsize) & 0xffff0000);

	memcpy((void *) loadaddr, rfx + 1, rfx->imgsize);
	memset((void *) loadaddr + rfx->imgsize, 0, rfx->memsize - rfx->imgsize);

	relocs = (void *)(rfx + 1) + rfx->imgsize;

	for(indx = 0; indx < rfx->nrelocs; ++indx) {

		type = relocs[indx] & 3;
		pfix = (void *) loadaddr + (relocs[indx] & ~3);
		data = *pfix;

		switch(type) {

			case RFX_REL_32:
				*pfix = data + loadaddr;
				break;

			case RFX_REL_26:
				data += (loadaddr & 0x0fffffff) >> 2;
				if((*pfix ^ data) & 0xfc000000) {
					puts("chain: RFX_REL_26 relocation out of range");
					fatal();
				}
				*pfix = data;
				break;

			case RFX_REL_H16:
				*pfix = (data & 0xffff0000) | ((data + (loadaddr >> 16)) & 0x0000ffff);
				break;

			default:
				puts("chain: unexpected RFX relocation");
				fatal();
		}
	}
	
	/* ensure what we just loaded is in physical memory */

	dcache_flush_all();

	/* jump to it */

	puts("chain: starting stage2");
	drain();

	((void (*)(size_t, size_t, unsigned))(rfx->entry + loadaddr))(ram[0], ram[1], switches);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
