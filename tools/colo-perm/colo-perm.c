/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define APP_NAME					"colo-perm"

#define COLO_SIG_OFFSET			4
#define COLO_SIG_STRING			"CoLo"
#define COLO_SIG_SIZE			(sizeof(COLO_SIG_STRING) - 1)

#define FLASH_TOTAL_SIZE		(512 << 10)
#define FLASH_BASE_OFFSET		0x1fc00000

static void usage(void)
{
	puts("\nusage: " APP_NAME " [ -q ]\n");

	exit(255);
}

int main(int argc, char *argv[])
{
	int opt, fd, quiet;
	unsigned indx;
	char *flash;

	quiet = 0;
	opterr = 0;

	for(;;) {

		opt = getopt(argc, argv, "q");
		if(opt == -1)
			break;

		switch(opt) {

			case 'q':
				quiet = 1;
				break;

			default:
				usage();
		}
	}

	if(argc != optind)
		usage();

	fd = open("/dev/mem", O_RDONLY | O_SYNC);
	if(fd == -1) {
		fprintf(stderr, APP_NAME ": failed to open /dev/mem (%s)\n", strerror(errno));
		return 255;
	}

	flash = mmap(NULL, FLASH_TOTAL_SIZE, PROT_READ, MAP_SHARED, fd, FLASH_BASE_OFFSET);
	if(flash == MAP_FAILED) {
		fprintf(stderr, APP_NAME ": failed to map /dev/mem (%s)\n", strerror(errno));
		return 255;
	}

	if(memcmp(flash + COLO_SIG_OFFSET, COLO_SIG_STRING, COLO_SIG_SIZE)) {
		if(!quiet)
			puts(APP_NAME ": \"CoLo\" not resident");
		return 1;
	}

	if(!quiet) {
		fputs(APP_NAME ": \"CoLo\" is resident <", stdout);
		for(indx = COLO_SIG_OFFSET; flash[indx] && indx < COLO_SIG_OFFSET + 40; ++indx)
			putchar(isprint(flash[indx]) ? flash[indx] : '?');
		puts(">");
	}
	
	return 0;
}

/* vi:set ts=3 sw=3 cin: */
