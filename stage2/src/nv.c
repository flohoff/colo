/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "galileo.h"

#define MENU_TIMEOUT				(10 * 1000)

#define RTC_ADDR_STORE			0x20

struct nv_store nv_store;

/*
 * write byte to RTC
 */
void rtc_write(unsigned addr, unsigned data)
{
	BRDG_ISA_SPACE[0x70] = 0x80 | addr;
	udelay(2);
	BRDG_ISA_SPACE[0x71] = data;
	udelay(2);
}

/*
 * read byte from RTC
 */
unsigned rtc_read(unsigned addr)
{
	unsigned data;

	BRDG_ISA_SPACE[0x70] = 0x80 | addr;
	udelay(2);
	data = BRDG_ISA_SPACE[0x71];
	udelay(2);

	return data;
}

/*
 * check/write CRC for block
 */
static unsigned nv_crc(struct nv_store *store)
{
	unsigned crc, indx, loop, data;

	crc = 0;

	for(indx = 1; indx < store->size; ++indx) {

		data = ((uint8_t *) store)[indx];

		for(loop = 8; loop--; data >>= 1) {

			if((data ^ crc) & 1)
				crc = 0x80 | ((crc ^ 0x18) >> 1);
			else
				crc >>= 1;

			data >>= 1;
		}
	}

	return crc;
}

/*
 * load 'nv_store' from RTC RAM
 */
void nv_get(int clear)
{
	unsigned indx;

	memset(&nv_store, 0, sizeof(nv_store));
	
	if(!clear) {

		for(indx = 0; indx < sizeof(nv_store); ++indx)
			((uint8_t *) &nv_store)[indx] = rtc_read(RTC_ADDR_STORE + indx);

		if(nv_store.size < 3 || nv_store.size > sizeof(nv_store) || nv_store.crc != nv_crc(&nv_store))
			memset(&nv_store, 0, sizeof(nv_store));

		if(nv_store.vers == NV_STORE_VERSION)
			return;
	}

	if(nv_store.vers < 3) {

		nv_store.keymap = 0;

		nv_store.vers = 3;
	}

	nv_put();
}

/*
 * store 'nv_store' to RTC RAM
 */
void nv_put(void)
{
	unsigned indx;

	nv_store.vers = NV_STORE_VERSION;
	nv_store.size = sizeof(nv_store);
	nv_store.crc = nv_crc(&nv_store);

	for(indx = 0; indx < sizeof(nv_store); ++indx)
		rtc_write(RTC_ADDR_STORE + indx, ((uint8_t *) &nv_store)[indx]);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
