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
#define LCD_ROW_OFFSET				0x40

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
	static unsigned indx;

	if(!indx) {

		LCD_WRITE(0, LCD_CGRAM_ADDR | 8);

		for(indx = 0; indx < sizeof(data); ++indx)
			LCD_WRITE(1, data[indx]);
	}
}

/*
 * run menu on front panel
 */
int lcd_menu(const char **options, unsigned count, unsigned timeout)
{
	unsigned mark, done, row, top, btn;
	int prv;

	if(count < 2)
		return -1;

	lcd_prog_chars();

	prv = -1;

	row = 1;
	top = 0;

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

			if(timeout && done > timeout)
				return LCD_MENU_TIMEOUT;

			for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < ((CP0_COUNT_RATE + 500) / 1000) * BUTTON_DEBOUNCE;)
				if(BREAK())
					return LCD_MENU_ABORT;

			btn = ~BUTTONS() & BUTTON_MASK;

			if(prv < 0) {
				if(btn)
					continue;
				prv = btn;
			}

			btn ^= prv;
			prv ^= btn;
			btn &= prv;

			if(btn & (BUTTON_RIGHT | BUTTON_ENTER | BUTTON_SELECT))
				return top + row - 1;

			if(btn & BUTTON_LEFT)
				return LCD_MENU_CANCEL;

			if(btn & BUTTON_UP) {
				if(top) {
					--top;
					row = 1;
					break;
				}
				if(row != 1) {
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

/*
 * 'menu' shell command
 */
int cmnd_menu(int opsz)
{
	static char buf[16];
	unsigned timeout;
	int which;
	char *ptr;

	if(argc < 4)
		return E_ARGS_UNDER;

	timeout = strtoul(argv[2], &ptr, 10);
	if(*ptr || ptr == argv[2])
		return E_BAD_VALUE;

	argv[2] = argv[1];

	switch(which = lcd_menu((const char **) &argv[2], argc - 2, timeout * 100))
	{
		case LCD_MENU_ABORT:
			env_put("menu-option", NULL, 0);
			return E_UNSPEC;

		case LCD_MENU_TIMEOUT:
		case LCD_MENU_CANCEL:
			which = 0;
			break;

		default:
			++which;
	}

	sprintf(buf, "%d", which);
	env_put("menu-option", buf, VAR_OTHER);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
