/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"

/*
 * flush address range from D-cache
 */
void dcache_flush(unsigned long addr, unsigned count)
{
	void *line, *end;

	count += addr % DCACHE_LINE_SIZE;
	line = KSEG0(addr - addr % DCACHE_LINE_SIZE);

	for(end = line + count; line < end; line += DCACHE_LINE_SIZE)
		CACHE(CACHE_HitWritebackInvD, line);
}

/*
 * flush entire D-cache
 */
void dcache_flush_all(void)
{
	void *line, *end;

	line = KSEG0(0);

	for(end = line + DCACHE_TOTAL_SIZE; line < end; line += DCACHE_LINE_SIZE)
		CACHE(CACHE_IndexWritebackInvD, line);
}

/*
 * flush entire I-cache
 */
void icache_flush_all(void)
{
	void *line, *end;

	line = KSEG0(0);

	for(end = line + ICACHE_TOTAL_SIZE; line < end; line += ICACHE_LINE_SIZE)
		CACHE(CACHE_IndexInvalidateI, line);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
