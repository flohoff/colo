/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage1/src/lcd.c,v 1.2 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "galileo.h"

#define LCD_TIMEOUT					(CP0_COUNT_RATE/50)	// 20ms
#define LCD_COLUMNS					16

#define LCD_BASE						((volatile unsigned *) BRDG_NCS3_BASE)
#define _LCD_WRITE(r,d)				do{LCD_BASE[!!(r)*4]=(unsigned)(d)<<24;}while(0)
#define _LCD_READ(r)					(LCD_BASE[!!(r)*4]>>24)
#define LCD_WRITE(r,d)				do{lcd_wait();_LCD_WRITE((r),(d));}while(0)
#define LCD_READ(r)					({lcd_wait();_LCD_READ(r);})

#define LCD_CLEAR						0x01
#define LCD_ENTRY_MODE_SET			(0x04 | (1 << 1))
#define LCD_DISPLAY_OFF				0x08
#define LCD_DISPLAY_ON				(LCD_DISPLAY_OFF | (1 << 2))
#define LCD_CURSOR_ON				(LCD_DISPLAY_ON | (1 << 1))
#define LCD_CURSOR_BLINK			(LCD_CURSOR_ON | (1 << 0))
#define LCD_FUNCTION_SET			(0x20 | (1 << 4) | (1 << 3))
#define LCD_DDRAM_ADDR				0x80

/*
 * wait for LCD ready
 */
static void lcd_wait(void)
{
	unsigned mark;

	for(mark = MFC0(CP0_COUNT); (_LCD_READ(0) & 0x80) && MFC0(CP0_COUNT) - mark < LCD_TIMEOUT;)
		udelay(2);

	udelay(10);
}

/*
 * initialise LCD
 */
void lcd_init(void)
{
	udelay(30000);
	_LCD_WRITE(0, LCD_FUNCTION_SET);
	udelay(8200);
	_LCD_WRITE(0, LCD_FUNCTION_SET);
	udelay(200);
	_LCD_WRITE(0, LCD_FUNCTION_SET);
	udelay(200);

	LCD_WRITE(0, LCD_FUNCTION_SET);
	LCD_WRITE(0, LCD_CLEAR);
	LCD_WRITE(0, LCD_DISPLAY_ON);
	LCD_WRITE(0, LCD_ENTRY_MODE_SET);
}

/*
 * write text to a line of the LCD
 */
void lcd_line(int row, const char *str)
{
	unsigned indx;

	LCD_WRITE(0, LCD_DDRAM_ADDR | 40 * !!row);

	indx = 0;

	if(str)
		for(; str[indx] && indx < LCD_COLUMNS; ++indx)
			LCD_WRITE(1, str[indx]);

	for(; indx < LCD_COLUMNS; ++indx)
		LCD_WRITE(1, ' ');
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
