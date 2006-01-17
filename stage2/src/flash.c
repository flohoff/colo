/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

#define FLASH_BASE				0x1fc00000
#define FLASH_P					((volatile uint8_t *) KSEG1(FLASH_BASE))

#define UNLOCK1(v)				do{FLASH_P[0x5555]=(v);}while(0)
#define UNLOCK2(v)				do{FLASH_P[0x2aaa]=(v);}while(0)
#define UNLOCK1_VALUE			0xaa
#define UNLOCK2_VALUE			0x55

#define CMND_ERASE_SECTOR		0x30
#define CMND_ERASE_SETUP		0x80
#define CMND_AUTOSELECT			0x90
#define CMND_PROGRAM				0xa0
#define CMND_RESET				0xf0

#define AUTOSELECT_VENDOR		0
#define AUTOSELECT_DEVICE		1

#define ERASE_TIMEOUT			15000		/* 15s  */
#define PROGRAM_TIMEOUT			10000		/* 10ms */

#define DEVICE_AM29F040			0x01a4	/* and TMS29F040 */

#define FLASH_SIZE				(512 << 10)

/*
 * erase Flash block
 */
static int flash_erase(unsigned long addr)
{
	unsigned prev, curr, tick, test, nbad;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_ERASE_SETUP);
	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);

	FLASH_P[addr] = CMND_ERASE_SECTOR;

	tick = ERASE_TIMEOUT;
	nbad = 0;

	for(prev = FLASH_P[addr];;) {

		curr = FLASH_P[addr];
		test = (curr ^ prev) & (1 << 6);
		if(!test)
			break;
		if(prev & curr & (1 << 5)) {
			if(++nbad == 10)
				break;
		} else
			nbad = 0;
		prev = curr;

		if(!tick)
			break;
		--tick;

		udelay(1000);
	}

	UNLOCK1(CMND_RESET);

	return !test;
}

/*
 * program single Flash byte
 */
static int flash_program_byte(unsigned long addr, unsigned data)
{
	unsigned prev, curr, test, tick, nbad;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_PROGRAM);
	
	FLASH_P[addr] = data;

	tick = PROGRAM_TIMEOUT;
	nbad = 0;

	for(prev = FLASH_P[addr];;) {

		curr = FLASH_P[addr];
		test = (curr ^ prev) & (1 << 6);
		if(!test)
			break;
		if(prev & curr & (1 << 5)) {
			if(++nbad == 10)
				break;
		} else
			nbad = 0;
		prev = curr;

		if(!tick)
			break;
		--tick;

		udelay(1);
	}

	UNLOCK1(CMND_RESET);

	return !test && FLASH_P[addr] == data;
}

/*
 * program block to Flash, erasing as necessary
 */
static int flash_program_block(unsigned long addr, const void *data, size_t size)
{
	unsigned indx;

	/* erase any blocks which we can't overwrite */

	for(indx = 0; indx < size; ++indx)

		if(((uint8_t *) data)[indx] & ~FLASH_P[addr + indx]) {

			putchar('*');

			if(!flash_erase(addr + indx))
				return addr + indx;

			/* skip on a bit */

			indx = ((addr + indx) | 0xfff) + 1 - addr;
		}

	/* now write our data in */

	for(indx = 0; indx < size; ++indx) {

		if(!(indx & 0x1fff))
			putchar('+');

		if(!flash_program_byte(addr + indx, ((uint8_t *) data)[indx]))
			return addr + indx;
	}

	return -1;
}

/*
 * find out if address is in a locked block
 */
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

/*
 * return device identifier
 */
static unsigned flash_ident(void)
{
	unsigned ident;

	UNLOCK1(UNLOCK1_VALUE);
	UNLOCK2(UNLOCK2_VALUE);
	UNLOCK1(CMND_AUTOSELECT);

	ident = FLASH_P[AUTOSELECT_VENDOR];
	ident = FLASH_P[AUTOSELECT_DEVICE] | (ident << 8);

	UNLOCK1(CMND_RESET);

	return ident;
}

int cmnd_flash(int opsz)
{
	unsigned long addr, targ, mark;
	unsigned indx, ident, key;
	size_t size;
	char *ptr;
	int stat;

	indx = 1;

	if(argc > 2) {

		if(argc < 4)
			return E_ARGS_COUNT;
		if(argc > 4)
			return E_ARGS_OVER;

		addr = evaluate(argv[indx++], &ptr);
		if(*ptr)
			return E_BAD_EXPR;

		size = evaluate(argv[indx++], &ptr);
		if(*ptr)
			return E_BAD_EXPR;

		if(!size || (unsigned long) KPHYS(addr) + size > ram_size)
			return E_BAD_VALUE;

	} else {

		if(argc < 2)
			return E_ARGS_UNDER;

		addr = (unsigned long) heap_image(&size);

		if(!size) {
			puts("do data loaded");
			return E_UNSPEC;
		}
	}

	targ = evaluate(argv[indx], &ptr);
	if(*ptr)
		return E_BAD_EXPR;

	targ = (unsigned long) KPHYS(targ);

	if(targ >= FLASH_BASE)
		targ -= FLASH_BASE;

	if(targ + size > FLASH_SIZE)
		return E_BAD_VALUE;

	ident = flash_ident();
	if(ident != DEVICE_AM29F040) {
		printf("unrecognised device %04x\n", ident);
		udelay(1000000);
	}

	key = (MFC0(CP0_COUNT) / 16) % 26 + 'A';
	printf("press <%c> to proceed...", key);

	while(kbhit())
		getch();

	for(mark = MFC0(CP0_COUNT); !kbhit();)
		if(MFC0(CP0_COUNT) - mark >= 2 * CP0_COUNT_RATE) {
			puts("\naborted");
			return E_UNSPEC;
		}

	putchar('\n');

	if(getch() != key) {
		puts("aborted");
		return E_UNSPEC;
	}

	stat = flash_program_block(targ, KSEG0(addr), size);

	putchar('\n');

	if(stat >= 0) {
		printf("programming failed at offset %06x\n", stat);
		if(flash_locked(stat))
			puts("** block is locked **");
		return E_UNSPEC;
	}

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
