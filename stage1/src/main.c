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

unsigned switches;
size_t mem_bank[2];

/*
 * initialise data segment
 */
static void crt_init(void)
{
	extern char __data, __edata, __etext;
	extern char __bss, __ebss;
	unsigned indx;

	for(indx = 0; indx < &__edata - &__data; ++indx)
		(&__data)[indx] = (&__etext)[indx];

	for(indx = 0; indx < &__ebss - &__bss; ++indx)
		(&__bss)[indx] = 0;
}

/*
 * convert value to ASCII decimal
 */
char *to_decimal(char *ptr, unsigned value)
{
	if(value > 9)
		ptr = to_decimal(ptr, value / 10);
	*ptr++ = value % 10 | '0';

	return ptr;
}

/*
 * convert value to ASCII hex
 */
char *to_hex(char *ptr, unsigned value, unsigned count)
{
	if(count > 1)
		ptr = to_hex(ptr, value >> 4, count - 1);
	value = (value & 0xf) | '0';
	*ptr++ = value + (value > '9') * 7;

	return ptr;
}

/*
 * format DRAM configuration for display
 */
static const char *dram_config(size_t *bank, size_t size)
{
	static char buf[16];
	char *ptr;

	ptr = to_decimal(buf, bank[0] >> 20);
	*ptr++ = '/';
	ptr = to_decimal(ptr, bank[1] >> 20);
	*ptr++ = ',';
	ptr = to_decimal(ptr, bank[2] >> 20);
	*ptr++ = '/';
	ptr = to_decimal(ptr, bank[3] >> 20);
	*ptr++ = '-';
	*ptr++ = '>';
	*to_decimal(ptr, size >> 20) = '\0';

	return buf;
}

/*
 * initialise memory and stuff
 */
void *stage1(void)
{
	size_t size[4];
	size_t *bank;

	crt_init();

	/* set device chip select configurations */

	BRDG_REG_WORD(BRDG_REG_DEV_PARM_NCSBOOT) = BRDG_NCSBOOT_CONFIG;
	BRDG_REG_WORD(BRDG_REG_DEV_PARM_NCS0) = BRDG_NCS0_CONFIG;
	BRDG_REG_WORD(BRDG_REG_DEV_PARM_NCS1) = BRDG_NCS1_CONFIG;
	BRDG_REG_WORD(BRDG_REG_DEV_PARM_NCS2) = BRDG_NCS2_CONFIG;
	BRDG_REG_WORD(BRDG_REG_DEV_PARM_NCS3) = BRDG_NCS3_CONFIG;

	/* read state of buttons */

	switches = *(volatile unsigned *) BRDG_NCS2_BASE >> 24;

	/* all LEDs off */

	*(volatile uint8_t *) BRDG_NCS0_BASE = 0;

	lcd_init();

	/* initialise memory */

	lcd_line(0, "Booting...");

	bank = dram_init(size);

	lcd_line(1, dram_config(size, bank[0] + bank[1]));

	if(bank[0] + bank[1] == 0) {

		lcd_line(0, "!MEMORY FAILURE!");
		fatal();
	}

	mem_bank[0] = bank[0];
	mem_bank[1] = bank[1];

	/* unlock D-cache */

	MTC0(CP0_STATUS, CP0_STATUS_BEV);

	/* returns new SP */

	return KSEG0(mem_bank[0] + mem_bank[1]);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
