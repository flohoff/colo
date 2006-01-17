/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "linux/ext2_fs.h"

#define BLOCK_COUNT					24
#define BLOCK_SIZE					EXT2_MAX_BLOCK_SIZE

static unsigned head;

static struct
{
	void				*device;
	unsigned long	block;
	unsigned			size;
	int				next;
	uint8_t			data[BLOCK_SIZE];

} cache[BLOCK_COUNT];

/*
 * flush block cache
 */
void block_flush(void *device)
{
	unsigned indx;

	for(indx = 0; indx < BLOCK_COUNT; ++indx)
		if(!device || cache[indx].device == device)
			cache[indx].device = NULL;
}

/*
 * read block via cache
 */
void *block_read(void *device, unsigned long block, size_t size, size_t hwsize)
{
	unsigned blksz, count, offset;
	int which, tail, prev;

	assert(size <= BLOCK_SIZE && hwsize <= BLOCK_SIZE && hwsize);

	blksz = size;
	offset = 0;

	/* if block size is smaller than sector size cache whole sectors */

	count = hwsize / size;
	if(count > 1) {
		blksz = hwsize;
		offset = (block % count) * size;
		block /= count;
	}

	/* search for block, MRU first */

	prev = 0;
	tail = 0;
	for(which = cache[head].next; which >= 0; which = cache[which].next) {
		if(cache[which].device == device && cache[which].block == block && cache[which].size == blksz)
			return cache[which].data + offset;
		prev = tail;
		tail = which;
	}

	/* block not found, replace LRU */

	cache[tail].device = device;
	cache[tail].block = block;
	cache[tail].size = blksz;

	count = blksz / hwsize;
	if(ide_read_sectors(device, cache[tail].data, block * count, count)) {
		cache[tail].device = NULL;
		return NULL;
	}

	/* move to MRU */

	cache[prev].next = -1;
	cache[tail].next = head;
	head = tail;

	return cache[tail].data + offset;
}

/*
 * read block from disk avoiding cache if we can
 */
int block_read_raw(void *device, void *buf, unsigned long block, size_t size, size_t hwsize)
{
	unsigned count;
	void *data;

	count = size / hwsize;
	if(count)
		return !ide_read_sectors(device, buf, block * count, count);

	/* block size is smaller than sector size so use cache to split sector */

	data = block_read(device, block, size, hwsize);
	if(!data)
		return 0;

	memcpy(buf, data, size);

	return 1;
}

/*
 * initialise block cache
 */
int block_init(void)
{
	int indx;

	/* initialise MRU list */

	for(indx = BLOCK_COUNT; indx--;)
		cache[indx].next = indx - 1;
	head = BLOCK_COUNT - 1;

	block_flush(NULL);

	return 1;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
