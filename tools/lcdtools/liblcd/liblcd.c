/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <stdlib.h>
#include <assert.h>

#include "liblcd.h"

static const struct lcd_dispatch_table *f;

void lcd_prog(unsigned code, const void *data)
{
	if(f && f->prog_char)
		f->prog_char(code, data);
}

void lcd_puts(unsigned row, unsigned col, unsigned wid, const char *str)
{
	if(f && f->puts)
		f->puts(row, col, wid, str);
}

void lcd_clear(void)
{
	if(f && f->clear)
		f->clear();
}

void lcd_curs_move(unsigned row, unsigned col)
{
	if(f && f->curs_move)
		f->curs_move(row, col);
}

void lcd_text(const char *str, unsigned max)
{
	if(f && f->text)
		f->text(str, max);
}

int btn_read(void)
{
	return (f && f->buttons) ? f->buttons() : 0;
}

int lcd_open(const char *dev)
{
	f = lcddev_open(dev);
	if(!f)
		f = lcdraw_open();

	return f != NULL;
}

void lcd_close(void)
{
	if(f) {
		f->close();
		f = NULL;
	}
}

/* vi:set ts=3 sw=3 cin: */
