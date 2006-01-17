/*
 * (C) P.Horton 2004,2005,2006
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

#include "lib.h"
#include "md5.h"

#define APP_NAME					"copy-rom"

#define ROM_PHYS_ADDR			0x1fc00000
#define ROM_PHYS_SIZE			(512 << 10)

static int usage(void)
{
	puts("usage: " APP_NAME " [ file ]");

	return 1;
}

int main(int argc, char *argv[])
{
	struct MD5Context ctx;
	uint8_t dig[16];
	unsigned indx;
	int fd, copy;
	void *rom;

	if(argc > 2)
		return usage();

	fd = open("/dev/mem", O_RDONLY | O_SYNC);
	if(fd == -1) {
		fprintf(stderr, APP_NAME ": failed to open /dev/mem (%s)\n", strerror(errno));
		return 1;
	}

	rom = mmap(NULL, ROM_PHYS_SIZE, PROT_READ, MAP_SHARED, fd, ROM_PHYS_ADDR);
	if(rom == MAP_FAILED) {
		fprintf(stderr, APP_NAME ": failed to map /dev/mem (%s)\n", strerror(errno));
		return 1;
	}

	close(fd);

	if(argc > 1) {

		fd = creat(argv[1], 0664);
		if(fd == -1) {
			fprintf(stderr, APP_NAME ": failed to create file (%s)\n", strerror(errno));
			return 1;
		}

		for(indx = 0; indx < ROM_PHYS_SIZE;) {

			copy = write(fd, rom + indx, ROM_PHYS_SIZE - indx);
			if(copy == -1) {
				if(errno != EINTR) {
					fprintf(stderr, APP_NAME ": failed writing file (%s)\n", strerror(errno));
					return 1;
				}
			} else
				indx += copy;
		}

		close(fd);
	}

	MD5Init(&ctx);
	MD5Update(&ctx, rom, ROM_PHYS_SIZE);
	MD5Final(dig, &ctx);

	for(indx = 0; indx < sizeof(dig); ++indx)
		printf("%02x", dig[indx]);
	putchar('\n');

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
