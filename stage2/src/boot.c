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
/*  |------------| */

	"BOOT SELECTION",
	"Disk    (hda)",
	"Network (NFS)",
	"Network (TFTP)",
	"Boot shell",
};

static const char *script[] =
{
	/* Disk (hda) */

	"lcd 'Booting...'\n"
	"mount\n"
	"lcd 'Booting...' /dev/{mounted-volume}\n"
	"-load /boot/default.colo\n"
	"-script\n"
	"load /boot/vmlinux.gz\n"
	"execute",

	/* Network (NFS) */

	"lcd 'Booting...'\n"
	"net\n"
	"lcd 'Booting...' {ip-address}\n"
	"nfs {dhcp-next-server} {dhcp-root-path} {dhcp-boot-file}\n"
	"-script\n"
	"execute",

	/* Network (TFTP) */

	"lcd 'Booting...'\n"
	"net\n"
	"lcd 'Booting...' {ip-address}\n"
	"tftp {dhcp-next-server} {dhcp-boot-file}\n"
	"-script\n"
	"execute",
};

void boot(int which)
{
	static char buf[16];

	if(which < 0)
		which = nv_store.boot;

	if(!which) {

		DPUTS("boot: running boot menu");

		which = lcd_menu(option, elements(option), MENU_TIMEOUT);

		switch(which) {

			case LCD_MENU_TIMEOUT:
			case LCD_MENU_CANCEL:
				which = nv_store.boot;
				break;

			case LCD_MENU_ABORT:
				puts("aborted");
				return;

			default:
				++which;
		}
	}

	if(which)
		--which;

	sprintf(buf, "%d", which);
	env_put("boot-option", buf, VAR_OTHER);

	if(which < elements(script))
		script_exec(script[which]);
	else
		DPRINTF("boot: no script #%d\n", which);
}

int cmnd_boot(int opsz)
{
	unsigned indx, size;
	int which, list;
	char *ptr;

	which = nv_store.boot;

	if(argc > 1) {

		if(argc > 3)
			return E_ARGS_OVER;

		size = strlen(argv[1]);

		list = !strncasecmp(argv[1], "list", size);

		if(argc == 2 && list) {

			for(indx = 1; indx <= elements(option); ++indx)
				printf("%x: %c %s\n", indx, (indx == nv_store.boot ? '*' : '.'), option[indx - 1]);

			return E_NONE;
		}

		which = evaluate(argv[argc - 1], &ptr);
		if(*ptr || which < 0 || which > elements(option))
			return E_BAD_VALUE;

		if(argc == 3 && !strncasecmp(argv[1], "default", size)) {
			nv_store.boot = which;
			nv_put();
			return E_NONE;
		}

		if(list) {

			if(!which)
				return E_BAD_VALUE;

			puts(option[--which]);
			printf("%.*s\n", (int) strlen(option[which]), "--------------");
			puts(script[which]);

			return E_NONE;
		}
	}

	boot(which);

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
