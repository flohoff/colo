/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "liblcd.h"

#define DEFAULT_DEVICE					"/dev/lcd"

#define LCD_ROW_OFFSET					0x40

/* -----  device driver interface  ----- */

#define LCD_WIDTH_MAX					40

#define LCD_Clear							3
#define LCD_Write							13
#define BUTTON_Read						50

struct lcd_display
{
	unsigned long	buttons;
	int				size1;
	int				size2;
	unsigned char	line1[LCD_WIDTH_MAX];
	unsigned char	line2[LCD_WIDTH_MAX];
	unsigned char	cursor_address;
	unsigned char	character;
	unsigned char	leds;
	unsigned char	*RomImage;
};

/* ------------------------------------- */

static struct lcd_display info;
static unsigned xpos, ypos;
static int dev;

static void exp_clear(void)
{
	ioctl(dev, LCD_Clear);

	memset(info.line1, ' ', LCD_WIDTH);
	memset(info.line2, ' ', LCD_WIDTH);
	
	xpos = 0;
	ypos = 0;
}

static void exp_curs_move(unsigned row, unsigned col)
{
	xpos = col;
	ypos = row;
}

static void exp_text(const char *str, unsigned wid)
{
	unsigned char *ptr;

	ypos += xpos / LCD_WIDTH;
	xpos %= LCD_WIDTH;
	ypos %= 2;

	if(wid < LCD_WIDTH)
		wid += xpos;
	if(wid > LCD_WIDTH)
		wid = LCD_WIDTH;

	info.size1 = 0;
	info.size2 = 0;

	if(ypos)
		ptr = info.line2;
	else
		ptr = info.line1;

	while(xpos < wid && *str)
		ptr[xpos++] = *str++;

	if(ypos)
		info.size2 = xpos;
	else
		info.size1 = xpos;

	ioctl(dev, LCD_Write, &info);
}

static void exp_puts(unsigned row, unsigned col, unsigned wid, const char *str)
{
	unsigned char *ptr;

	exp_curs_move(row, col);

	ypos += xpos / LCD_WIDTH;
	xpos %= LCD_WIDTH;
	ypos %= 2;

	if(wid < LCD_WIDTH)
		wid += xpos;
	if(wid > LCD_WIDTH)
		wid = LCD_WIDTH;

	info.size1 = 0;
	info.size2 = 0;

	if(ypos)
		ptr = info.line2;
	else
		ptr = info.line1;

	while(xpos < wid && *str)
		ptr[xpos++] = *str++;

	while(xpos < wid)
		ptr[xpos++] = ' ';

	if(ypos)
		info.size2 = xpos;
	else
		info.size1 = xpos;

	ioctl(dev, LCD_Write, &info);
}

static int exp_buttons(void)
{
	struct lcd_display tmp;

	return ioctl(dev, BUTTON_Read, &tmp) ? 0 : (~tmp.buttons & BTN_MASK);
}

static void exp_close(void)
{
	close(dev);
}

static const struct lcd_dispatch_table funcs =
{
	.close		= exp_close,
	.clear		= exp_clear,
	.puts			= exp_puts,
	.text			= exp_text,
	.curs_move	= exp_curs_move,
	.buttons		= exp_buttons,
};

const struct lcd_dispatch_table *lcddev_open(const char *name)
{
	if(!name)
		name = DEFAULT_DEVICE;

	dev = open(name, O_RDWR);
	if(dev == -1) {
		fprintf(stderr, "%s: failed to open %s (%s)\n", getapp(), name, strerror(errno));
		return NULL;
	}

	memset(info.line1, ' ', LCD_WIDTH);
	memset(info.line2, ' ', LCD_WIDTH);

	return &funcs;
}

/* vi:set ts=3 sw=3 cin: */
