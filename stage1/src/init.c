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

#define LED_QUBE_LEFT				(1 << 0)
#define LED_QUBE_RIGHT				(1 << 1)
#define LED_RAQ_WEB					(1 << 2)
#define LED_RAQ_POWER_OFF			(1 << 3)

/*
 * initialise CPU and lock data segment into D-cache
 */
void cpu_init(void)
{
	extern char __data;
	void *line, *end;
	unsigned indx;

	/* ensure STATUS register is sane */

	MTC0(CP0_STATUS, CP0_STATUS_BEV | CP0_STATUS_ERL);

	/* set KSEG0 for writeback caching */

	MTC0(CP0_CONFIG, CP0_CONFIG_K0_WRITEBACK);

	/* zap the caches (same size and line size) */

	MTC0(CP0_TAGLO, 0);

	line = KSEG0(0);
	for(end = line + DCACHE_TOTAL_SIZE; line < end; line += DCACHE_LINE_SIZE) {
		CACHE(CACHE_IndexStoreTagD, line);
		CACHE(CACHE_IndexStoreTagI, line);
	}

	/* fill first way of the D-cache */

	for(indx = 0; indx < DCACHE_TOTAL_SIZE / DCACHE_WAY_COUNT; indx += DCACHE_LINE_SIZE)
		*(volatile unsigned *)(&__data + indx);

	/* lock first way of the D-cache */

	MTC0(CP0_STATUS, CP0_STATUS_DL | CP0_STATUS_BEV | CP0_STATUS_ERL);

	NOP(); NOP(); NOP(); NOP();

	/* flush the TLB */

	MTC0(CP0_ENTRYLO0, 0);
	MTC0(CP0_ENTRYLO1, 0);

	for(indx = 0; indx < TLB_ENTRY_COUNT; ++indx) {
		MTC0(CP0_ENTRYHI, (indx * (4 << 10) * 2) | (unsigned long) KSEG0(0));
		MTC0(CP0_INDEX, indx);
		TLBWI();
	}

	/* drop exception level */

	MTC0(CP0_STATUS, CP0_STATUS_DL | CP0_STATUS_BEV);
}

/*
 * nothing else to do but hang
 */
void fatal(void)
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
 * yikes! we've taken an exception
 *
 * hope we got past LCD initialisation
 */
void exception(unsigned long vect)
{
	static char buf0[] = "!EXCEPTION #x  !";
	static char buf1[] = "EPC xxxxxxxx xxx";
	unsigned long epc;
	unsigned cause;

	epc = MFC0(CP0_EPC);
	cause = MFC0(CP0_CAUSE);
	if(cause & CP0_CAUSE_BD)
		epc += 4;
	cause = (cause >> 2) & 0x1f;

	to_decimal(buf0 + 12, cause);
	to_hex(buf1 + 4, epc, 8);
	to_hex(buf1 + 13, vect, 3);

	lcd_line(0, buf0);
	lcd_line(1, buf1);

	fatal();
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
