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
#include <stdlib.h>
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

static void usage(void)
{
	puts("\nusage: " APP_NAME " [ -n ] line1 [ line2 ] \n");

	exit(-1);
}

int main(int argc, char *argv[])
{
	int option, erase;
	char *text;

	erase = 1;

	opterr = 0;

	while((option = getopt(argc, argv, "n")) != -1)
		switch(option)
		{
			case 'n':
				erase = 0;
				break;

			default:
				usage();
		}

	if(optind + 2 < argc)
		usage();

	if(!lcd_open(NULL))
		return -1;

	text = "";
	if(argc > optind)
		text = argv[optind];

	if(erase || text[0])
		lcd_puts(0, 0, LCD_WIDTH, text);

	++optind;

	text = "";
	if(argc > optind)
		text = argv[optind];

	if(erase || text[0])
		lcd_puts(1, 0, LCD_WIDTH, text);

	lcd_close();

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
