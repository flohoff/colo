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

#define BUTTONS()						(*(volatile unsigned *) BRDG_NCS2_BASE >> 24)

#define BUTTON_DEBOUNCE				50

#define LCD_TIMEOUT					(CP0_COUNT_RATE/50)	// 20ms
#define LCD_COLUMNS					16
#define LCD_ROW_OFFSET				40

#define LCD_BASE						((volatile unsigned *) BRDG_NCS3_BASE)
#define _LCD_WRITE(r,d)				do{LCD_BASE[!!(r)*4]=(unsigned)(d)<<24;}while(0)
#define _LCD_READ(r)					(LCD_BASE[!!(r)*4]>>24)
#define LCD_WRITE(r,d)				do{lcd_wait();_LCD_WRITE((r),(d));}while(0)
#define LCD_READ(r)					({lcd_wait();_LCD_READ(r);})

#define LCD_BUSY						(1 << 7)

#define LCD_CGRAM_ADDR				0x40
#define LCD_DDRAM_ADDR				0x80

/*
 * wait for LCD ready
 */
static void lcd_wait(void)
{
	unsigned mark;

	for(mark = MFC0(CP0_COUNT); (_LCD_READ(0) & LCD_BUSY) && MFC0(CP0_COUNT) - mark < LCD_TIMEOUT;)
		udelay(2);

	udelay(10);
}

/*
 * write text to LCD
 */
static void lcd_text(const char *str, int size)
{
	unsigned indx;

	indx = 0;

	if(str)
		for(; indx < (unsigned) size && str[indx]; ++indx)
			LCD_WRITE(1, str[indx]);

	for(; indx < size; ++indx)
		LCD_WRITE(1, ' ');
}

/*
 * write text to a line of the LCD
 */
void lcd_line(int row, const char *str)
{
	LCD_WRITE(0, LCD_DDRAM_ADDR | (LCD_ROW_OFFSET * !!row));

	lcd_text(str, LCD_COLUMNS);
}

/*
 * load programmable characters
 */
static void lcd_prog_chars(void)
{
	static uint8_t data[] = {
		0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01, 0x00,		/* right arrow */
		0x10, 0x18, 0x1c, 0x1e, 0x1c, 0x18, 0x10, 0x00,		/* left arrow  */
	};
	unsigned indx;

	LCD_WRITE(0, LCD_CGRAM_ADDR | 8);

	for(indx = 0; indx < sizeof(data); ++indx)
		LCD_WRITE(1, data[indx]);
}

/*
 * run menu on front panel
 */
int lcd_menu(const char **options, unsigned count, unsigned which, unsigned timeout)
{
	unsigned mark, done, row, top, btn, prv;

	if(count < 2 || which >= count)
		return -1;

	lcd_prog_chars();

	prv = BUTTONS();

	row = (which == count - 1);
	top = which - row;

	for(;;) {

		LCD_WRITE(0, LCD_DDRAM_ADDR);
		LCD_WRITE(1, row ? ' ' : '\002');
		lcd_text(options[top], LCD_COLUMNS - 2);
		LCD_WRITE(1, row ? ' ' : '\001');

		LCD_WRITE(0, LCD_DDRAM_ADDR | LCD_ROW_OFFSET);
		LCD_WRITE(1, row ? '\002' : ' ');
		lcd_text(options[top + 1], LCD_COLUMNS - 2);
		LCD_WRITE(1, row ? '\001' : ' ');

		for(done = 0;; done += BUTTON_DEBOUNCE) {

			if(done > timeout)
				return which;

			for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < ((CP0_COUNT_RATE + 500) / 1000) * BUTTON_DEBOUNCE;)
				yield();

			btn = BUTTONS();

			btn ^= prv;
			prv ^= btn;
			btn &= ~prv;

			if(btn & (BUTTON_RIGHT | BUTTON_ENTER | BUTTON_SELECT))
				return top + row;

			if(btn & BUTTON_LEFT)
				return -1;

			if(btn & BUTTON_UP) {
				if(top) {
					--top;
					row = 1;
					break;
				}
				if(row) {
					row = 0;
					break;
				}
			}

			if(btn & BUTTON_DOWN) {
				if(top + 2 < count) {
					++top;
					row = 0;
					break;
				} 
				if(!row) {
					row = 1;
					break;
				}
			}
		}
	}
}

/*
 * 'lcd' shell command
 */
int cmnd_lcd(int opsz)
{
	if(argc > 3)
		return E_ARGS_OVER;

	lcd_line(0, argc > 1 ? argv[1] : NULL);
	lcd_line(1, argc > 2 ? argv[2] : NULL);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
