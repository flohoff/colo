/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

#define MENU_TIMEOUT						(10 * 1000)

static const char *option[] =
{
/*  |-------------| */

	"Disk    (hda)",
	"Network (TFTP)",
	"Network (NFS)",
	"Boot shell",
};

static const char *script[] =
{
	"mount\n-script /boot/default.colo\nload /boot/vmlinux.gz\nexecute",
	NULL,
	NULL,
	NULL,
};

void boot(int which)
{
	if(which < 0) {
		DPUTS("boot: running boot menu");
		which = lcd_menu(option, elements(option), 0, MENU_TIMEOUT);
		if(which < 0)
			which = 0;
	}

	shell(which < elements(script) ? script[which] : NULL);
}

int cmnd_boot(int opsz)
{
	unsigned indx, size;
	int which, list;
	char *ptr;

	if(argc == 1)
		boot(0);

	if(argc > 3)
		return E_ARGS_OVER;

	size = strlen(argv[1]);

	list = !strncasecmp(argv[1], "list", size);

	if(argc == 2) {

		if(list) {
			for(indx = 0; indx < elements(option); ++indx)
				printf("%x: %s\n", indx, option[indx]);
			return E_NONE;
		}

		if(!strncasecmp(argv[1], "menu", size))
			boot(-1);
	}

	which = evaluate(argv[argc - 1], &ptr);
	if(*ptr || which < 0 || which >= elements(script))
		return E_BAD_VALUE;

	if(list) {
		if(which < elements(option)) {
			puts(option[which]);
			printf("%.*s\n", (int) strlen(option[which]), "--------------");
		}
		puts(script[which]);
		return E_NONE;
	}

	boot(which);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
