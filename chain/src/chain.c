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
 * display error message and die
 */
static void loader_error(const char *msg)
{
	lcd_line(0, "!RFX LOAD FAIL !");
	lcd_line(1, msg);
	fatal();
}

/*
 * convert value to ASCII decimal
 */
static char *to_decimal(char *ptr, unsigned value)
{
	if(value > 9)
		ptr = to_decimal(ptr, value / 10);
	*ptr++ = value % 10 | '0';

	return ptr;
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
static void memory_info(size_t *bank)
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
 * show memory configuration
 */
static const char *memory_config(size_t *bank)
{
	static char buf[16];
	char *ptr;

	ptr = to_decimal(buf, bank[0] >> 20);
	*ptr++ = ',';
	ptr = to_decimal(ptr, bank[1] >> 20);
	*ptr++ = '-';
	*ptr++ = '>';
	ptr = to_decimal(ptr, (bank[0] + bank[1]) >> 20);
	*ptr = '\0';

	return buf;
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

	/* read state of buttons */

	switches = *(volatile unsigned *) BRDG_NCS2_BASE >> 24;

	/* all LEDs off */

	*(volatile uint8_t *) BRDG_NCS0_BASE = 0;

	/* get memory bank sizes */

	memory_info(ram);

	if(KSEG0(ram[0] + ram[1]) != (void *) arg) {
		lcd_line(0, "! MEMORY SIZE  !");
		lcd_line(1, "!   MISMATCH   !");
		fatal();
	}

	lcd_line(0, "Booting...");
	lcd_line(1, memory_config(ram));

	/* load stage2 */

	rfx = (void *) &__stage2;
	
	if(memcmp(rfx->magic, RFX_HDR_MAGIC, RFX_HDR_MAGIC_SZ))
		loader_error(" INVALID HEADER");

	loadaddr = ram[0] + ram[1] - (32 << 10); // XXX

	if(rfx->memsize > loadaddr)
		loader_error(" OUT OF MEMORY");

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
				if((*pfix ^ data) & 0xfc000000)
					loader_error(" BAD RELOCATION");
				*pfix = data;
				break;

			case RFX_REL_H16:
				*pfix = (data & 0xffff0000) | ((data + (loadaddr >> 16)) & 0x0000ffff);
				break;

			default:
				loader_error(" UNKNOWN RELOC");
		}
	}
	
	/* ensure what we just loaded is in physical memory */

	dcache_flush_all();

	/* jump to it */

	((void (*)(size_t, size_t, unsigned))(rfx->entry + loadaddr))(ram[0], ram[1], switches);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
