/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include "liblcd.h"

#define APP_NAME					"putlcd"

const char *getapp(void)
{
	return APP_NAME;
}

int main(int argc, char *argv[])
{
	if(!lcd_open(NULL))
		return -1;

	lcd_puts(0, 0, LCD_WIDTH, argc > 1 ? argv[1] : "");
	lcd_puts(1, 0, LCD_WIDTH, argc > 2 ? argv[2] : "");

	lcd_close();

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
