/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "linux/ext2_fs.h"
#include "cpu.h"

#define SCRATCH_SIZE			EXT2_MAX_BLOCK_SIZE
#define SYMLINK_PATH_MAX	100

static char scratch[SCRATCH_SIZE];

struct volume
{
	void							*device;
	unsigned						sector_size;
	unsigned						block_size;
	unsigned						super_block;
	unsigned						large_file_mask;
	unsigned						ea_blocks;
	int							journal;
	int							mounted;
	struct ext2_super_block	super;
};

struct find_item
{
	const char			*glob;
	struct ext2_inode	*dir;
	unsigned				offset;
	unsigned				inode;
	char					name[EXT2_NAME_LEN + 1];
};

/*
 * read block via block cache
 */
static void *ext2_read_block(struct volume *v, unsigned long block)
{
	static uint8_t zero[EXT2_MAX_BLOCK_SIZE];

	assert(v->mounted);

	if(block >= v->super.s_blocks_count) {
		DPUTS("ext2: block out of range");
		return NULL;
	}

	if(!block)
		return zero;

	return block_read(v->device, block, v->block_size, v->sector_size);
}

/*
 * read block direct
 */
static int ext2_read_block_raw(struct volume *v, void *data, unsigned long block)
{
	assert(v->mounted);

	if(block >= v->super.s_blocks_count) {
		DPUTS("ext2: block out of range");
		return 0;
	}

	if(!block) {
		memset(data, 0, v->block_size);
		return 1;
	}

	return !!block_read_raw(v->device, data, block, v->block_size, v->sector_size);
}

/*
 * mount volume
 */
int ext2_mount(struct volume *v, unsigned block)
{
	void *sb;

	if(!block)
		++block;

	v->block_size = EXT2_MIN_BLOCK_SIZE;
	v->super.s_blocks_count = block + 1;
	v->mounted = 1;

	sb = ext2_read_block(v, block);

	v->mounted = 0;

	if(!sb)
		return 0;

	memcpy(&v->super, sb, sizeof(v->super));

	v->block_size = 1 << EXT2_BLOCK_SIZE_BITS(&v->super);

	if(v->super.s_magic != EXT2_SUPER_MAGIC ||
		v->block_size < EXT2_MIN_BLOCK_SIZE ||
		v->block_size > EXT2_MAX_BLOCK_SIZE)
	{
		DPUTS("ext2: invalid super block");
		return 0;
	}

	v->super_block = block;
	if(v->super.s_log_block_size)
		v->super_block = (block >> v->super.s_log_block_size) + 1;
	
	v->large_file_mask = 0;
	v->ea_blocks = 0;
	v->journal = 0;

	if(v->super.s_rev_level >= EXT2_DYNAMIC_REV) {

		if(v->super.s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE)
			v->large_file_mask = ~0;

		if(v->super.s_feature_compat & EXT2_FEATURE_COMPAT_EXT_ATTR)
			v->ea_blocks = v->block_size / 512;

		if(v->super.s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL)
			v->journal = 1;
	}

	DPRINTF("ext2: revision %d\n", v->super.s_rev_level);
	if(v->large_file_mask)
		DPUTS("ext2: large file support");
	if(v->ea_blocks)
		DPUTS("ext2: extended attributes");
	if(v->journal)
		DPUTS("ext2: has journal (ext3)");

	v->mounted = 1;

	return 1;
}

/*
 * unmount volume
 */
void ext2_umount(struct volume *v)
{
	if(!v->mounted) {
		DPUTS("ext2: not mounted");
		return;
	}

	block_flush(v->device);

	v->mounted = 0;
}

/*
 * get block number for logical block of file
 *
 * (returns 0 for sparse blocks)
 */
int ext2_block_map(struct volume *v, struct ext2_inode *inode, unsigned *map)
{
	unsigned block, shift, count, link[3];
	unsigned *indirect;

	block = *map;

	if(block < EXT2_NDIR_BLOCKS) {
		*map = inode->i_block[block];
		return 1;
	}
	block -= EXT2_NDIR_BLOCKS;

	shift = EXT2_BLOCK_SIZE_BITS(&v->super) - 2;
	 
	for(count = 0;;) {

		if(count == 3)
			return 0;

		link[count++] = block & ((1 << shift) - 1);
		block >>= shift;

		if(!block)
			break;
		--block;
	}

	for(block = inode->i_block[EXT2_IND_BLOCK + count - 1]; block && count;) {

		indirect = ext2_read_block(v, block);
		if(!indirect)
			return 0;

		block = indirect[link[--count]];
	}

	*map = block;

	return 1;
}

/*
 * fetch specified inode structure
 */
int ext2_inode_fetch(struct volume *v, struct ext2_inode *result, unsigned number)
{
	struct ext2_group_desc *gdescs;
	unsigned desc, group, block;
	struct ext2_inode *inodes;

	if(!number)
		return 0;

	desc = --number / v->super.s_inodes_per_group;
	number %= v->super.s_inodes_per_group;

	group = desc / EXT2_DESC_PER_BLOCK(&v->super);
	desc %= EXT2_DESC_PER_BLOCK(&v->super);

	block = number / (v->block_size / sizeof(struct ext2_inode));
	number %= v->block_size / sizeof(struct ext2_inode);

	gdescs = ext2_read_block(v, v->super_block + v->super.s_first_data_block + group);
	if(!gdescs)
		return 0;

	inodes = ext2_read_block(v, gdescs[desc].bg_inode_table + block);
	if(!inodes)
		return 0;

	*result = inodes[number];

	return 1;
}

/*
 * find subsequent entries in directory
 */
int ext2_find_next(struct volume *v, struct find_item *find)
{
	struct ext2_dir_entry_2 *entry;
	unsigned block, offset;
	char *direct;
	int match;

	do {

		if(find->offset >= find->dir->i_size)
			return 0;

		if(find->offset & EXT2_DIR_ROUND)
			return -1;

		block = find->offset / v->block_size;
		offset = find->offset % v->block_size;
		find->offset -= offset;

		if(!ext2_block_map(v, find->dir, &block))
			return -1;

		if(!block) {
			DPUTS("ext2: directory corrupt");
			return -1;
		}

		direct = ext2_read_block(v, block);
		if(!direct)
			return -1;

		do {

			entry = (struct ext2_dir_entry_2 *)(direct + offset);

			if((entry->rec_len & EXT2_DIR_ROUND) || entry->rec_len < EXT2_DIR_REC_LEN(entry->name_len)) {
				DPUTS("ext2: directory entry corrupt");
				return -1;
			}

			match = !!entry->inode;
			if(match) {

				memcpy(find->name, entry->name, entry->name_len);
				find->name[entry->name_len] = '\0';

				if(find->glob)
					match = glob(find->name, find->glob);
			}

			offset += entry->rec_len;

		} while(!match && offset < v->block_size);

		find->offset += offset;

	} while(!match);

	find->inode = entry->inode;

	return 1;
}

/*
 * find first entry in directory
 */
int ext2_find_first(struct volume *v, struct find_item *find, struct ext2_inode *inode, const char *glob)
{
	if(!S_ISDIR(inode->i_mode)) {
		DPUTS("ext2: not a directory");
		return -1;
	}

	if(v->large_file_mask & inode->i_size_high) {
		DPUTS("ext2: directory too large");
		return -1;
	}

	if(inode->i_size & (v->block_size - 1)) {
		DPUTS("ext2: invalid directory length");
		return -1;
	}

	find->dir = inode;
	find->offset = 0;
	find->glob = glob;

	return ext2_find_next(v, find);
}

/*
 * read symlink contents
 */
int ext2_readlink(struct volume *v, char *link, size_t size, struct ext2_inode *inode)
{
	unsigned index, block;
	void *data;

	if(!size)
		return 0;

	if(!S_ISLNK(inode->i_mode)) {
		DPUTS("ext2: not a symlink");
		return 0;
	}

	if(v->large_file_mask & inode->i_size_high) {
		DPUTS("ext2: symlink too large");
		return 0;
	}

	if(inode->i_size < --size)
		size = inode->i_size;

	if(inode->i_blocks > (inode->i_file_acl ? v->ea_blocks : 0))

		for(index = 0; index < size; index += v->block_size) {

			block = index / v->block_size;

			if(!ext2_block_map(v, inode, &block))
				return 0;

			if(!block) {
				DPUTS("ext2: sparse block in symlink");
				return 0;
			}

			data = ext2_read_block(v, block);
			if(!data)
				return 0;

			memcpy(link + index, data + index, size - index);
		}

	else {

		if(sizeof(struct ext2_inode) - (size_t)((struct ext2_inode *) 0)->i_block < size) {
			DPUTS("ext2: invalid fast symlink");
			return 0;
		}

		memcpy(link, inode->i_block, size);
	}

	link[size] = '\0';

	return 1;
}

/*
 * follow path from inode returning target inode
 */
unsigned ext2_lookup(struct volume *v, unsigned inum, const char *path)
{
	unsigned size, curr, which, other, link, prev;
	struct ext2_inode inode[2];
	struct find_item find;
	int stat;

	size = strlen(path) + 1;
	if(size > sizeof(scratch)) {
		DPUTS("ext2: path too long");
		return 0;
	}

	curr = sizeof(scratch) - size;
	strcpy(&scratch[curr], path);

	if(scratch[curr] == '/')
		inum = EXT2_ROOT_INO;

	link = 0;
	which = 0;

	if(!ext2_inode_fetch(v, &inode[which], inum))
		return 0;

	for(;;) {

		for(; scratch[curr] == '/'; ++curr)
			;

		if(!scratch[curr])
			break;

		if(!S_ISDIR(inode[which].i_mode)) {
			DPUTS("ext2: not a directory");
			return 0;
		}

		for(size = curr; scratch[size] && scratch[size] != '/'; ++size)
			;
		size -= curr;

		for(stat = ext2_find_first(v, &find, &inode[which], NULL); stat > 0; stat = ext2_find_next(v, &find))
			if(!memcmp(&scratch[curr], find.name, size) && !find.name[size]) {
				curr += size;
				break;
			}

		other = 1 - which;

		if(stat < 1 || !ext2_inode_fetch(v, &inode[other], find.inode))
			return 0;

		if(S_ISLNK(inode[other].i_mode)) {

			if(++link == SYMLINK_PATH_MAX) {
				DPUTS("ext2: too many symlinks");
				return 0;
			}

			prev = curr;

			if(curr < inode[other].i_size || (v->large_file_mask & inode[other].i_size_high)) {
				DPUTS("ext2: symlinks too long");
				return 0;
			}
			curr -= inode[other].i_size;

			if(!ext2_readlink(v, &scratch[curr], inode[other].i_size + 1, &inode[other]))
				return 0;

			if(prev < sizeof(scratch) - 1)
				scratch[prev] = '/';

			DPRINTF("ext2: {%s%s} --> {%s}\n", find.name, &scratch[prev], &scratch[curr]);

			if(scratch[curr] == '/') {

				inum = EXT2_ROOT_INO;

				if(!ext2_inode_fetch(v, &inode[which], inum))
					return 0;
			}

		} else {

			inum = find.inode;
			which = other;
		}
	}

	return inum;
}

/*
 * get directory path from inode
 */
int ext2_dirpath(struct volume *v, char *path, size_t max, unsigned inum)
{
	unsigned parent, child, posn, size;
	struct ext2_inode inode;
	struct find_item find;
	int stat;

	child = 0;

	for(posn = max;;)
	{
		if(!ext2_inode_fetch(v, &inode, inum))
			return 0;

		if(!S_ISDIR(inode.i_mode)) {
			DPUTS("ext2: not a directory");
			return 0;
		}

		parent = 0;
		size = 0;

		for(stat = ext2_find_first(v, &find, &inode, NULL); stat > 0; stat = ext2_find_next(v, &find))
		{
			if(find.name[0] == '.' && find.name[1] == '.' && find.name[2] == '\0') {
				parent = find.inode;
				if(size || !child)
					break;
			}

			if(child && find.inode == child) {
			
				size = strlen(find.name);
				if(!size) {
					DPUTS("ext2: zero length filename");
					return 0;
				}
				if(posn <= size + 1) {
					DPUTS("ext2: path too long");
					return 0;
				}
				path[--posn] = '/';
				posn -= size;
				memcpy(path + posn, find.name, size);

				if(parent)
					break;
			}
		}

		if(stat < 1) {
			if(!stat)
				DPUTS("ext2: directory corrupt");
			return 0;
		}

		if(parent == inum)
			break;

		child = inum;
		inum = parent;
	}

	path[1] = '\0';
	if(posn < max) {
		memmove(path + 1, path + posn, max - posn);
		path[max - posn] = '\0';
	}
	path[0] = '/';

	return 1;
}

/* ------------------------------------------------------------------------ */

static struct volume vol;
static unsigned curdir;

/*
 * mount disk volume
 */
int cmnd_mount(int opsz)
{
	if(argc > 2)
		return E_ARGS_OVER;

	if(vol.mounted)
		ext2_umount(&vol);

	env_put("mounted-volume", NULL, VAR_OTHER);

	vol.device = ide_open(argc > 1 ? argv[1] : NULL);
	if(!vol.device)
		return E_UNSPEC;

	vol.sector_size = ide_block_size(vol.device);

	if(!ext2_mount(&vol, 0))
		return E_UNSPEC;

	curdir = EXT2_ROOT_INO;

	env_put("mounted-volume", ide_dev_name(vol.device), VAR_OTHER);

	return E_NONE;
}

/*
 * list inode details
 */
static int list_inode(unsigned inum, const char *name, int descend)
{
	static char node[16];

	struct ext2_inode inode;
	struct find_item find;
	int stat;

	if(!ext2_inode_fetch(&vol, &inode, inum))
		return 0;

	if(descend) {

		if(S_ISDIR(inode.i_mode)) {

			for(stat = ext2_find_first(&vol, &find, &inode, NULL); stat > 0; stat = ext2_find_next(&vol, &find))
				list_inode(find.inode, find.name, 0);

			return !stat;
		}
	}

	if(S_ISCHR(inode.i_mode) || S_ISBLK(inode.i_mode)) {
		sprintf(node, "%u,%4u", (inode.i_block[0] >> 8) & 0xff, inode.i_block[0] & 0xff);
		printf("%10s  ", node);
	} else if(vol.large_file_mask & inode.i_size_high)
		putstring("      >4GB  ");
	else
		printf("%10u  ", inode.i_size);

	putstring_safe(name, -1);

	if(S_ISLNK(inode.i_mode)) {
		putchar('@');
		if(ext2_readlink(&vol, scratch, sizeof(scratch), &inode)) {
			putstring(" --> ");
			putstring_safe(scratch, -1);
		}
	} else if(S_ISDIR(inode.i_mode))
		putchar('/');
	else if(S_ISFIFO(inode.i_mode))
		putchar('|');
	else if(S_ISSOCK(inode.i_mode))
		putchar('=');
	else if(inode.i_mode & 0111)
		putchar('*');

	putchar('\n');

	return 1;
}

/*
 * list current/specified directory/files
 */
int cmnd_ls(int opsz)
{
	unsigned indx, inum;
	char *path;

	if(!vol.mounted) {
		puts("not mounted");
		return E_UNSPEC;
	}

	path = ".";

	for(indx = 1;; ++indx) {

		if(indx < argc)
			path = argv[indx];

		inum = ext2_lookup(&vol, curdir, path);
		if(inum)
			list_inode(inum, path, 1);
		else
			printf("file not found \"%s\"\n", path);

		if(indx >= argc)
			break;
	}

	return E_NONE;
}

/*
 * change current directory
 */
int cmnd_cd(int opsz)
{
	struct ext2_inode inode;
	unsigned inum;

	if(argc > 1) {

		if(argc > 2)
			return E_ARGS_OVER;

		if(!vol.mounted) {
			puts("not mounted");
			return E_UNSPEC;
		}

		inum = ext2_lookup(&vol, curdir, argv[1]);
		if(!inum) {
			puts("directory not found");
			return E_UNSPEC;
		}

		if(!ext2_inode_fetch(&vol, &inode, inum))
			return E_UNSPEC;

		if(!S_ISDIR(inode.i_mode)) {
			puts("not a directory");
			return E_UNSPEC;
		}

		curdir = inum;
	}

	if(ext2_dirpath(&vol, scratch, sizeof(scratch), curdir))
		puts(scratch);

	return E_NONE;
}

/*
 * open file and return handle
 */
void *file_open(const char *path, unsigned long *size)
{
	struct ext2_inode inode;
	unsigned inum;

	if(!vol.mounted) {
		puts("not mounted");
		return NULL;
	}

	inum = ext2_lookup(&vol, curdir, path);
	if(!inum) {
		puts("file not found");
		return NULL;
	}

	if(!ext2_inode_fetch(&vol, &inode, inum))
		return NULL;

	if(!S_ISREG(inode.i_mode)) {
		puts("not a file");
		return NULL;
	}

	if(vol.large_file_mask & inode.i_size_high) {
		puts("file too large");
		return NULL;
	}

	if(size)
		*size = inode.i_size;

	return (void *) inum;
}

/*
 * load file into memory
 */
int file_load(void *hdl, void *where, unsigned long size)
{
	struct ext2_inode inode;
	unsigned long seek;
	unsigned block;
	void *copy;

	if(!ext2_inode_fetch(&vol, &inode, (unsigned) hdl))
		return 0;

	for(seek = 0; size;) {

		block = seek / vol.block_size;

		if(!ext2_block_map(&vol, &inode, &block))
			return 0;

		if(size < vol.block_size) {

			copy = ext2_read_block(&vol, block);
			if(!copy)
				return 0;

			memcpy(where + seek, copy, size);

			break;
		}

		if(!ext2_read_block_raw(&vol, where + seek, block))
			return 0;

		seek += vol.block_size;
		size -= vol.block_size;
	}

	return 1;
}

/*
 * load file from volume
 */
int cmnd_load(int opsz)
{
	unsigned long imagesz, initrdsz;
	void *himage, *hinitrd, *base;

	if(argc < 2)
		return E_ARGS_UNDER;

	hinitrd = NULL;

	if(argc > 2) {

		if(argc > 3)
			return E_ARGS_OVER;

		hinitrd = file_open(argv[2], &initrdsz);
		if(!hinitrd)
			return E_UNSPEC;
	}

	himage = file_open(argv[1], &imagesz);
	if(!himage)
		return E_UNSPEC;

	heap_reset();

	if(hinitrd) {

		base = heap_reserve_hi(initrdsz);
		if(!base) {
			puts("file too big");
			return E_UNSPEC;
		}

		if(!file_load(hinitrd, base, initrdsz))
			return E_UNSPEC;

		heap_alloc();
		heap_mark();
	}

	base = heap_reserve_hi(imagesz);
	if(!base) {
		puts("file too big");
		heap_reset();
		return E_UNSPEC;
	}

	if(!file_load(himage, base, imagesz)) {
		heap_reset();
		return E_UNSPEC;
	}

	heap_alloc();

	heap_initrd_vars();

	heap_info();

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
