/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "galileo.h"

/*
 * setup address decoding for specified DRAM group
 *
 * a 'size' of 0 will disable the group
 */
static void dram_decode(int group, unsigned long base, size_t size, int pair)
{
	unsigned offset;
	
	offset = (BRDG_REG_RAS23_LOW_DECODE - BRDG_REG_RAS01_LOW_DECODE) * !!group;

	BRDG_REG_WORD(BRDG_REG_RAS01_HIGH_DECODE + offset) = (base + (pair ? size * 2 : size) - 1) >> 21;
	BRDG_REG_WORD(BRDG_REG_RAS01_LOW_DECODE + offset) = base >> 21;

	BRDG_REG_WORD(BRDG_REG_RAS0_HIGH_DECODE + offset) = (base + size - 1) >> 20;
	BRDG_REG_WORD(BRDG_REG_RAS0_LOW_DECODE + offset) = base >> 20;

	/* if we've only one bank this will be outside the group window */

	base += size;

	BRDG_REG_WORD(BRDG_REG_RAS1_HIGH_DECODE + offset) = (base + size - 1) >> 20;
	BRDG_REG_WORD(BRDG_REG_RAS1_LOW_DECODE + offset) = base >> 20;
}

/*
 * find size of DRAM bank
 */
static size_t dram_size(unsigned long base, size_t size, unsigned offset)
{
	static const unsigned map[8] = {
		[0] = 12, [4] = 11, [6] = 10, [7] = 9,
	};
	unsigned mask, rbits, cbits;
	volatile unsigned *ram;

	/* set row size to 12 bits (and minimum cycle counts) */

	BRDG_REG_WORD(offset) = (12 - 9) << 4;
	
	/* find unused address lines */

	ram = (volatile unsigned *) KSEG1(base);

	ram[0] = 0;
	for(mask = sizeof(unsigned); mask < size; mask <<= 1)
		ram[mask / sizeof(unsigned)] |= mask;
	mask = ram[0];

	/* calculate row/column sizes */

	rbits = mask & (7 << 14);
	mask ^= rbits;
	rbits = map[rbits >> 14];

	cbits = mask & (7 << 23);
	mask ^= cbits;
	cbits = map[cbits >> 23];

	if(mask || !rbits || !cbits)
		return 0;

	/* quick test just to make sure there's RAM there */

	for(mask = 1; mask; mask <<= 1)
		*ram++ = mask;

	for(mask = ~0; mask; mask <<= 1)
		*ram++ = mask;

	ram = (volatile unsigned *) KSEG1(base);

	for(mask = 1; mask; mask <<= 1)
		if(*ram++ != mask)
			return 0;

	for(mask = ~0; mask; mask <<= 1)
		if(*ram++ != mask)
			return 0;

	/* set correct row size (and minimum cycle counts) */

	BRDG_REG_WORD(offset) = (rbits - 9) << 4;

	return 1 << (rbits + cbits + 2);
}

/*
 * initialise DRAM
 */
size_t *dram_init(size_t *size)
{
#	define MB(x)			((unsigned)(x)<<20)

	static size_t bank[4];

	if(!size)
		size = bank;

	/* refresh 4096 rows in 64ms (8/125s) */
	
	BRDG_REG_WORD(BRDG_REG_DRAM_CONFIG) = (BRDG_TCK * 8) / (125 * 4096);

	/* place DRAMs in address space, 64MB spacing */

	dram_decode(1, MB(128), MB(64), 1);
	dram_decode(0, MB(0), MB(64), 1);

	/* size DRAM banks */

	size[0] = dram_size(MB(0), MB(64), BRDG_REG_DRAM_PARM_0);
	size[1] = dram_size(MB(64), MB(64), BRDG_REG_DRAM_PARM_1);
	size[2] = dram_size(MB(128), MB(64), BRDG_REG_DRAM_PARM_2);
	size[3] = dram_size(MB(192), MB(64), BRDG_REG_DRAM_PARM_3);

	/* ignore SIMMs with bank mismatches */

	if(!size[1] || size[1] == size[0]) {
		bank[0] = size[0];
		bank[1] = size[1];
	} else {
		bank[0] = 0;
		bank[1] = 0;
	}

	if(!size[3] || size[3] == size[2]) {
		bank[2] = size[2];
		bank[3] = size[3];
	} else {
		bank[2] = 0;
		bank[3] = 0;
	}

	/* place DRAMs in CPU address space, largest bank at lowest address */

	if(bank[0] + bank[1] >= bank[2] + bank[3]) {
		dram_decode(0, 0, bank[0], !!bank[1]);
		dram_decode(1, bank[0] + bank[1], bank[2], !!bank[3]);
	} else {
		dram_decode(0, bank[2] + bank[3], bank[0], !!bank[1]);
		dram_decode(1, 0, bank[2], !!bank[3]);
	}

	bank[0] = bank[0] + bank[1];
	bank[1] = bank[2] + bank[3];

	return bank;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
