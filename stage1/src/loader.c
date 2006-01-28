/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "rfx.h"

extern size_t mem_bank[2];

void *(*memcpy_w)(void *, const void *, size_t)	= _memcpy_w;
void *(*memset_w)(void *, int, size_t)				= _memset_w;

/*
 * flush entire D-cache
 */
static void dcache_flush_all(void)
{
	void *line, *end;

	line = KSEG0(0);

	for(end = line + DCACHE_TOTAL_SIZE; line < end; line += DCACHE_LINE_SIZE)
		CACHE(CACHE_IndexWritebackInvD, line);
}

/*
 * display error message and die
 */
static void loader_error(const char *msg)
{
	lcd_line(0, "!RFX LOAD FAIL !");
	lcd_line(1, msg);
	fatal();
}

/*
 * load boot loader from RFX image in Flash
 */
void loader(void)
{
	extern char __stage2;

	unsigned indx, type, data;
	unsigned *pfix, *relocs;
	unsigned long loadaddr;
	struct rfx_header *rfx;

	/* ensure _memcpy_w() / _memset_w() are in physical memory */

	dcache_flush_all();

	rfx = KSEG1(&__stage2);
	
	for(indx = 0; indx < RFX_HDR_MAGIC_SZ; ++indx)
		if(rfx->magic[indx] != RFX_HDR_MAGIC[indx])
			loader_error(" INVALID HEADER");

	loadaddr = mem_bank[0] + mem_bank[1] - (32 << 10); // XXX

	if(rfx->memsize > loadaddr)
		loader_error(" OUT OF MEMORY");

	loadaddr = (unsigned long) KSEG0((loadaddr - rfx->memsize) & 0xffff0000);

	memcpy_w((void *) loadaddr, rfx + 1, rfx->imgsize);
	memset_w((void *) loadaddr + rfx->imgsize, 0, rfx->memsize - rfx->imgsize);

	relocs = (void *)(rfx + 1) + rfx->imgsize;

	for(indx = 0; indx < rfx->nrelocs; ++indx) {

		type = relocs[indx] & 3;
		pfix = (void *) loadaddr + (relocs[indx] & ~3);
		data = *pfix;

		switch(type) {

			case RFX_REL_32:
				*pfix = data + loadaddr;
				break;

			case RFX_REL_26:
				data += (loadaddr & 0x0fffffff) >> 2;
				if((*pfix ^ data) & 0xfc000000)
					loader_error(" BAD RELOCATION");
				*pfix = data;
				break;

			case RFX_REL_H16:
				*pfix = (data & 0xffff0000) | ((data + (loadaddr >> 16)) & 0x0000ffff);
				break;

			default:
				loader_error(" UNKNOWN RELOC");
		}
	}
	
	/* ensure what we just loaded is in physical memory */

	dcache_flush_all();

	/* jump to it */

	((void (*)(size_t, size_t, unsigned))(rfx->entry + loadaddr))(mem_bank[0], mem_bank[1], switches);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
