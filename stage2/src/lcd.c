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

static int lcd_menu_init(const char **, unsigned, unsigned);

int (*lcd_menu)(const char **, unsigned, unsigned) = lcd_menu_init;

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
		0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01, 0x00,		/* left arrow */
		0x10, 0x18, 0x1c, 0x1e, 0x1c, 0x18, 0x10, 0x00,		/* right arrow  */
	};
	static unsigned indx;

	if(!indx) {

		LCD_WRITE(0, LCD_CGRAM_ADDR | 8);

		for(indx = 0; indx < sizeof(data); ++indx)
			LCD_WRITE(1, data[indx]);
	}
}

/*
 * centre string in buffer (pad with spaces)
 */
static void lcd_centre(char *buf, const char *str, unsigned siz)
{
	unsigned ofs, len, idx;

	ofs = 0;
	len = strlen(str);
	if(len < siz)
		ofs = (siz - len) / 2;

	for(idx = 0; idx < siz; ++idx) {

		buf[idx] = ' ';
		if(idx >= ofs && idx < ofs + len)
			buf[idx] = *str++;
	}
}

/*
 * scroll display replacing current contents
 */
static void lcd_scroll(const char *str, int dir)
{
	static char lcd[LCD_COLUMNS - 2];
	char buf[LCD_COLUMNS - 2];
	unsigned idx, num, mark;

	lcd_centre(buf, str, sizeof(buf));

	if(!dir) {

		memcpy(lcd, buf, sizeof(lcd));

		lcd_prog_chars();

		LCD_WRITE(0, LCD_DDRAM_ADDR | LCD_ROW_OFFSET);
		LCD_WRITE(1, '\001');
		lcd_text(lcd, LCD_COLUMNS - 2);
		LCD_WRITE(1, '\002');

		return;
	}

	for(num = sizeof(lcd);;) {

		if(dir < 0) {

			for(idx = 1; idx < sizeof(lcd); ++idx)
				lcd[idx - 1] = lcd[idx];
			lcd[sizeof(lcd) - 1] = buf[sizeof(lcd) - num];

		} else {

			for(idx = sizeof(lcd); --idx;)
				lcd[idx] = lcd[idx - 1];
			lcd[0] = buf[num - 1];
		}

		LCD_WRITE(0, (LCD_DDRAM_ADDR | LCD_ROW_OFFSET) + 1);
		lcd_text(lcd, LCD_COLUMNS - 2);

		if(!--num)
			break;

		for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE / 33;)
			;
	}
}

/*
 * run scrolling horizontal menu on front panel
 */
static int lcd_menu_horz(const char **options, unsigned count, unsigned timeout)
{
	unsigned done, mark, sel, btn, prv;
	char buf[LCD_COLUMNS];
	int dir;

	if(count < 2)
		return LCD_MENU_ABORT;

	lcd_centre(buf, options[0], sizeof(buf));
	LCD_WRITE(0, LCD_DDRAM_ADDR);
	lcd_text(buf, sizeof(buf));

	lcd_scroll(options[1], 0);

	prv = -1;
	sel = 1;

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

		if(btn)
			done = 0;

		if(btn & (BUTTON_ENTER | BUTTON_SELECT))
			return sel - 1;

		if(btn & (BUTTON_UP | BUTTON_DOWN))
			return LCD_MENU_CANCEL;

		if(btn & (BUTTON_LEFT | BUTTON_RIGHT)) {

			dir = 1;
			if(btn & BUTTON_RIGHT)
				dir = -1;

			sel -= dir;
			if(sel < 1 || sel >= count)
				sel = count - sel - dir;

			lcd_scroll(options[sel], dir);
		}
	}
}

/*
 * run vertical menu on front panel
 */
static int lcd_menu_vert(const char **options, unsigned count, unsigned timeout)
{
	unsigned mark, done, row, top, btn;
	int prv;

	if(count < 2)
		return LCD_MENU_ABORT;

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

			if(btn)
				done = 0;

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
 * select menu type from NV and run menu
 */
static int lcd_menu_init(const char **options, unsigned count, unsigned timeout)
{
	lcd_menu = (nv_store.flags & NVFLAG_HORZ_MENU) ? lcd_menu_horz : lcd_menu_vert;

	return lcd_menu(options, count, timeout);
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
 * 'select' shell command
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
