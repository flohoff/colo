/*
 * (C) P.Horton 2004
 *
 * $Id$
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>

#define VER_MAJOR					1
#define VER_MINOR					3

#define APP_NAME					"flash-tool"

#define FLASH_TOTAL_SIZE		(512 << 10)
#define FLASH_BASE_OFFSET		0x1fc00000

#define UNLOCK1(v)				do{FLASH_P[0x555]=(v);}while(0)
#define UNLOCK2(v)				do{FLASH_P[0x2aa]=(v);}while(0)
#define UNLOCK1_VALUE			0xaa
#define UNLOCK2_VALUE			0x55

#define CMND_ERASE_SECTOR		0x30
#define CMND_ERASE_SETUP		0x80
#define CMND_AUTOSELECT			0x90
#define CMND_PROGRAM				0xa0
#define CMND_RESET				0xf0

#define ERASE_TIMEOUT			5

#define DEVICE_AM29F040			0x01a4

#define __STR(x)					#x
#define _STR(x)					__STR(x)

static volatile uint8_t *FLASH_P;

static unsigned flash_id(void)
{
	unsigned id;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_AUTOSELECT);

	id = FLASH_P[0];
	id = FLASH_P[1] | (id << 8);

	UNLOCK1(CMND_RESET);

	return id;
}

static int flash_locked(unsigned long addr)
{
	int lock;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_AUTOSELECT);

	lock = FLASH_P[addr];

	UNLOCK1(CMND_RESET);

	return lock;
}

static int flash_erase(unsigned long addr)
{
	unsigned nbad, prev, curr, test;
	time_t mark;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_ERASE_SETUP);
	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);

	FLASH_P[addr] = CMND_ERASE_SECTOR;

	nbad = 0;
	prev = FLASH_P[addr];
	mark = time(NULL);

	for(;;) {

		curr = FLASH_P[addr];

		test = (curr ^ prev) & (1 << 6);
		if(!test)
			break;

		if(prev & curr & (1 << 5)) {
			if(++nbad == 10)
				break;
		} else
			nbad = 0;

		if(time(NULL) - mark > ERASE_TIMEOUT)
			break;

		prev = curr;
	}

	UNLOCK1(CMND_RESET);

	return !test;
}

static int flash_program_byte(unsigned long addr, unsigned data)
{
	unsigned nbad, prev, curr, test;
	time_t mark;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_PROGRAM);

	FLASH_P[addr] = data;

	nbad = 0;
	prev = FLASH_P[addr];
	mark = time(NULL);

	for(;;) {

		curr = FLASH_P[addr];

		test = (curr ^ prev) & (1 << 6);
		if(!test)
			break;

		if(prev & curr & (1 << 5)) {
			if(++nbad == 10)
				break;
		} else
			nbad = 0;

		if(time(NULL) - mark > 1)
			break;

		prev = curr;
	}

	UNLOCK1(CMND_RESET);

	return !test && FLASH_P[addr] == data;
}

static int flash_program_block(unsigned long addr, const void *data, size_t size)
{
	unsigned indx;

	for(indx = 0; indx < size; ++indx) {

		if(((uint8_t *) data)[indx] & ~FLASH_P[addr + indx]) {

			putchar('*');
			fflush(stdout);

			if(!flash_erase(addr + indx))
				return addr + indx;

			indx = ((addr + indx) | 0xfff) + 1 - addr;
		}
	}

	for(indx = 0; indx < size; ++indx) {

		if(!(indx & 0x1fff)) {
			putchar('+');
			fflush(stdout);
		}

		if(!flash_program_byte(addr + indx, ((uint8_t *) data)[indx]))
			return addr + indx;
	}

	return -1;
}

static int key_wait(void)
{
	struct termios term, save;
	struct timeval tv;
	char match, key;
	int done;

	gettimeofday(&tv, NULL);
	match = ((tv.tv_sec * 100 + tv.tv_usec / 10000) ^ tv.tv_sec) % 26 + 'A';

	printf("press <%c> to proceed ...", match);
	fflush(stdout);

	tcgetattr(0, &term);
	save = term;

	term.c_lflag &= ~(ICANON | ECHO);
	term.c_cc[VMIN] = 0;
	term.c_cc[VTIME] = 2 * 10;
	tcsetattr(0, TCSANOW, &term);

	tcflush(0, TCIFLUSH);
	do
		done = read(0, &key, 1);
	while(done != 1 && errno == EINTR);
	
	tcsetattr(0, TCSANOW, &save);

	putchar('\n');

	return done == 1 && key == match;
}

static void usage(void)
{
	puts("\nusage: " APP_NAME " [ -w ] [ -o offset ] file\n\n  version " _STR(VER_MAJOR) "." _STR(VER_MINOR) "\n");

	exit(1);
}

int main(int argc, char *argv[])
{
	int opt, burn, fd, done, force;
	unsigned long offset;
	unsigned id, indx;
	off_t size;
	void *data;
	char *ptr;

	burn = 0;
	force = 0;
	offset = 0;
	opterr = 0;

	for(;;) {

		opt = getopt(argc, argv, "o:wF");
		if(opt == -1)
			break;

		switch(opt) {

			case 'o':
				offset = strtoul(optarg, &ptr, 0);
				if(ptr == optarg || *ptr) {
					fputs(APP_NAME ": invalid offset value\n", stderr);
					return 1;
				}
				break;

			case 'w':
				burn = 1;
				break;

			case 'F':
				force = 1;
				break;

			default:
				usage();
		}
	}

	if(argc - optind != 1)
		usage();

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd == -1) {
		fprintf(stderr, APP_NAME ": failed to open /dev/mem (%s)\n", strerror(errno));
		return 1;
	}

	FLASH_P = mmap(NULL, FLASH_TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FLASH_BASE_OFFSET);
	if(FLASH_P == MAP_FAILED) {
		fprintf(stderr, APP_NAME ": failed to map /dev/mem (%s)\n", strerror(errno));
		return 1;
	}

	id = flash_id();
	if(id != DEVICE_AM29F040) {
		fprintf(stderr, APP_NAME ": unexpected Flash device 0x%04x\n", id);
		return 1;
	}

	if(burn) {
		
		fd = open(argv[optind], O_RDONLY);
		if(fd == -1) {
			fprintf(stderr, APP_NAME ": failed to open %s (%s)\n", argv[optind], strerror(errno));
			return 1;
		}

		size = lseek(fd, 0, SEEK_END);
		if(size == (off_t) -1) {
			fprintf(stderr, APP_NAME ": seek failed in %s (%s)\n", argv[optind], strerror(errno));
			return 1;
		}

		if(offset + size > FLASH_TOTAL_SIZE) {
			fputs(APP_NAME ": data won't fit in Flash\n", stderr);
			return 1;
		}

		data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		if(data == MAP_FAILED) {
			fprintf(stderr, APP_NAME ": failed to map %s (%s)\n", argv[optind], strerror(errno));
			return 1;
		}

		if(!force && !key_wait()) {
			puts("aborted");
			return 1;
		}

		done = flash_program_block(offset, data, size);

		putchar('\n');

		if(done >= 0) {

			fprintf(stderr, flash_locked(done) ?
				APP_NAME ": programming failed at %06u, *** LOCKED ***\n" :
				APP_NAME ": programming failed at %06u\n",
				done);

			return 1;
		}

		if(memcmp((void *) FLASH_P + offset, data, size)) {
			fputs(APP_NAME ": VERIFY FAILED\n", stderr);
			return 1;
		}

		puts("programmed and verified successfully");

	} else {

		fd = creat(argv[optind], 0664);
		if(fd == -1) {
			fprintf(stderr, APP_NAME ": failed to create %s (%s)\n", argv[optind], strerror(errno));
			return 1;
		}

		for(indx = offset; indx < FLASH_TOTAL_SIZE;) {

			done = write(fd, (void *) FLASH_P + indx, FLASH_TOTAL_SIZE - indx);

			if(done == -1) {
				if(errno != EINTR) {
					fprintf(stderr, APP_NAME ": failed writing %s (%s)\n", argv[optind], strerror(errno));
					return -1;
				}
			} else
				indx += done;
		}

		close(fd);
	}

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
