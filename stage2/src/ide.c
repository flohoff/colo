/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage2/src/ide.c,v 1.3 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "pci.h"
#include "galileo.h"

#define TIMEOUT_RESET				(30 * 100)
#define TIMEOUT_IDENTIFY			(2 * 100)
#define TIMEOUT_DEV_DIAG			(5 * 100)
#define TIMEOUT_READ					(5 * 100)

#define IDE_REG_DATA					(*(volatile uint16_t *) &BRDG_ISA_SPACE[0x1f0])
#define IDE_REG_ERROR				(BRDG_ISA_SPACE[0x1f1])
#define IDE_REG_FEATURE				(BRDG_ISA_SPACE[0x1f1])
#define IDE_REG_NSECT				(BRDG_ISA_SPACE[0x1f2])
#define IDE_REG_SECTOR				(BRDG_ISA_SPACE[0x1f3])
#define IDE_REG_CYL_LO				(BRDG_ISA_SPACE[0x1f4])
#define IDE_REG_CYL_HI				(BRDG_ISA_SPACE[0x1f5])
#define IDE_REG_HEAD					(BRDG_ISA_SPACE[0x1f6])
# define REG_HEAD_SLAVE				(1 << 4)
# define REG_HEAD_LBA				(1 << 6)
# define REG_HEAD_DEFAULT			((1 << 7) | (1 << 5))
#define IDE_REG_STATUS				(BRDG_ISA_SPACE[0x1f7])
# define REG_STATUS_ERR				(1 << 0)
# define REG_STATUS_DRQ				(1 << 3)
# define REG_STATUS_BSY				(1 << 7)
#define IDE_REG_COMMAND				(BRDG_ISA_SPACE[0x1f7])
#define IDE_REG_STATUS_ALT			(BRDG_ISA_SPACE[0x3f6])
#define IDE_REG_CONTROL				(BRDG_ISA_SPACE[0x3f6])
# define REG_CONTROL_RESET			(1 << 2)
# define REG_CONTROL_DEFAULT		(1 << 3)

#define ATA_READ						0x20
#define ATA_READ_EXT					0x24
#define ATA_DEV_DIAG					0x90
#define ATA_IDENTIFY					0xec

#define FLAG_RESETTING				(1 << 0)
#define FLAG_IDENTIFIED				(1 << 1)
#define FLAG_LBA						(1 << 2)
#define FLAG_LBA_48					(1 << 3)

struct part_entry
{
	uint32_t boot:8;
	uint32_t start_chs:24;
	uint32_t type:8;
	uint32_t end_chs:24;
	uint32_t start_lba;
	uint32_t size_lba;

} __attribute__((packed));

static unsigned flags;
static unsigned long disk_size;
static unsigned disk_sects;
static unsigned disk_cyls;
static unsigned disk_heads;

static void ide_mode_4(void)
{
#	define PCI_CLOCKS(t)			((unsigned)((1LL*PCI_CLOCK*(t)+999999999)/1000000000))

#	define NCLKS_SETUP			PCI_CLOCKS(25)
#	define NCLKS_ACTIVE			PCI_CLOCKS(70)
#	define NCLKS_RECOVER			PCI_CLOCKS(25)

	DPUTS("ide: mode 4 timing");

	/* set port timing */

	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x4b, ((NCLKS_ACTIVE - 1) << 4) | (NCLKS_RECOVER - 1));
	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x4c, ((NCLKS_SETUP - 1) << 6) | 0x3f);
	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x4f, ((NCLKS_ACTIVE - 1) << 4) | (NCLKS_RECOVER - 1));

	/* enable prefetch buffer */

	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x41, 0x80 |
		pcicfg_read_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x41));
}

/*
 * reset drive
 */
static void ide_reset(void)
{
	IDE_REG_CONTROL = REG_CONTROL_DEFAULT | REG_CONTROL_RESET;
	udelay(10);
	IDE_REG_CONTROL = REG_CONTROL_DEFAULT;
	udelay(1000);

	flags = FLAG_RESETTING;
}

/*
 * issue ATA read command to drive
 */
static int ata_read(unsigned cmnd, void *data, unsigned count, unsigned timeout)
{
	unsigned stat, expire;
	unsigned long mark;
	void *end;

	assert((flags & FLAG_IDENTIFIED) || cmnd == ATA_DEV_DIAG || cmnd == ATA_IDENTIFY);
	assert(!((unsigned long) data & 3));
	
	if(IDE_REG_STATUS & (REG_STATUS_BSY | REG_STATUS_DRQ)) {
		printf("ide: %02x drive busy\n", cmnd);
		return -1;
	}

	IDE_REG_COMMAND = cmnd;
	udelay(1);

	for(;;) {

		expire = timeout;

		for(mark = MFC0(CP0_COUNT);;) {

			stat = IDE_REG_STATUS;
			if(!(stat & REG_STATUS_BSY)) {

				if(stat & REG_STATUS_ERR) {
					printf("ide: %02x error %02x\n", cmnd, IDE_REG_ERROR);
					return -1;
				}

				if(!count)
					return 0;

				if(stat & REG_STATUS_DRQ)
					break;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE / 100) {

				if((int) --expire <= 0) {
					printf("ide: %02x command timeout\n", cmnd);
					return -1;
				}

				mark += CP0_COUNT_RATE / 100;
			}
		}

		for(end = data + 512; data < end; data += 16) {

			((uint16_t *) data)[0x0] = IDE_REG_DATA;
			((uint16_t *) data)[0x1] = IDE_REG_DATA;
			((uint16_t *) data)[0x2] = IDE_REG_DATA;
			((uint16_t *) data)[0x3] = IDE_REG_DATA;
			((uint16_t *) data)[0x4] = IDE_REG_DATA;
			((uint16_t *) data)[0x5] = IDE_REG_DATA;
			((uint16_t *) data)[0x6] = IDE_REG_DATA;
			((uint16_t *) data)[0x7] = IDE_REG_DATA;
		}

		IDE_REG_STATUS_ALT;

		--count;
	}
}

/*
 * identify drive
 */
static int ide_identify(void)
{
	static union {
		uint32_t	w[512 / 4];
		uint16_t	h[1];
		uint8_t	b[1];
	} data;

	char model[(47 - 27) * 2];
	unsigned timeout, timing;
	unsigned long mark;

	/* if we're resetting wait for completion */

	if(flags & FLAG_RESETTING) {

		IDE_REG_HEAD = REG_HEAD_DEFAULT;			/* select master */

		timeout = TIMEOUT_RESET;

		for(mark = MFC0(CP0_COUNT);;)

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE / 100) {

				if(!(IDE_REG_STATUS & REG_STATUS_BSY))
					break;

				if((int) --timeout <= 0) {
					puts("ide: reset timeout");
					return -1;
				}

				mark += CP0_COUNT_RATE / 100;
			}

		DPUTS("ide: reset complete");
	}

	flags = 0;

	/* check there's a drive there */

	IDE_REG_CYL_LO = 0x55;
	IDE_REG_CYL_HI = 0x55;

	if(ata_read(ATA_DEV_DIAG, NULL, 0, TIMEOUT_DEV_DIAG))
		return -1;

	if((IDE_REG_ERROR & 0x7f) != 0x01 ||
		IDE_REG_NSECT != 0x01 || IDE_REG_SECTOR != 0x01 ||
		IDE_REG_CYL_LO != 0x00 || IDE_REG_CYL_HI != 0x00) {

		puts("ide: no drive found");
		return -1;
	}

	IDE_REG_HEAD = REG_HEAD_DEFAULT;			/* select master */

	DPUTS("ide: diagnostic complete");

	/* read IDENTIFY data */
	
	if(ata_read(ATA_IDENTIFY, data.b, 1, TIMEOUT_IDENTIFY))
		return -1;

	if(data.h[0] & (1 << 15)) {
		puts("ide: not ATA device");
		return -1;
	}

#ifdef _DEBUG
	{
		unsigned indx;

		for(indx = 0; indx < sizeof(model); ++indx)
			model[indx] = data.b[27 * 2 + (indx ^ 1)];
		while(indx && isspace(model[indx - 1]))
			--indx;

		putstring("ide: {");
		putstring_safe(model, indx);
		puts("}");
	}
#endif

	flags = 0;

	if((data.h[49] & (1 << 9)) &&
		!(debug_flags & DFLAG_IDE_DISABLE_LBA))
	{
		flags |= FLAG_LBA;
		if((data.h[83] & ((1 << 15) | (1 << 14) | (1 << 10))) == ((1 << 14) | (1 << 10)) &&
			!(debug_flags & DFLAG_IDE_DISABLE_LBA48))
		{
			flags |= FLAG_LBA_48;
			if(data.w[102 / 2]) {
				disk_size = ~0;
				puts("disk size capped at 2TB!");
			} else
				disk_size = data.w[100 / 2];
			DPRINTF("ide: LBA48 %lu\n", disk_size);
		} else {
			disk_size = data.w[60 / 2];
			DPRINTF("ide: LBA %lu\n", disk_size);
		}
	}

	if(!(flags & FLAG_LBA)) {
		disk_cyls = data.h[54];
		disk_heads = data.h[55];
		disk_sects = data.h[56];
		disk_size = (unsigned long) disk_cyls * disk_heads * disk_sects;
		if(!disk_size || disk_heads > 16 || disk_cyls > 65536 || disk_sects >= 256) {
			puts("ide: invalid CHS values");
			return -1;
		}
		DPRINTF("ide: CHS %u/%u/%u (%lu)\n", disk_cyls, disk_heads, disk_sects, disk_size);
	}

	if(data.h[53] & (1 << 0)) {
		timing = data.h[67];
		if(timing && timing <= 120 && !(debug_flags & DFLAG_IDE_DISABLE_TIMING))
			ide_mode_4();
	}

	flags |= FLAG_IDENTIFIED;

	return 0;
}

/*
 * read sectors from drive
 */
int ide_read_sectors(void *device, void *data, unsigned long addr, unsigned count)
{
	unsigned long sector;
	unsigned cmnd, nsect;

	assert(flags & FLAG_IDENTIFIED);

	if(device)
		addr += ((struct part_entry *) device)->start_lba;

	if(addr + count > disk_size) {
		puts("ide: attempt to read past end of disk");
		return -1;
	}

	for(cmnd = ATA_READ; count;) {

		if(flags & FLAG_LBA) {

			if(flags & FLAG_LBA_48) {

				cmnd = ATA_READ_EXT;

				IDE_REG_CYL_HI = 0;
				IDE_REG_CYL_LO = 0;
				IDE_REG_SECTOR = addr >> 24;

				IDE_REG_NSECT = 0;
			}

			IDE_REG_CYL_HI = addr >> 16;
			IDE_REG_CYL_LO = addr >> 8;
			IDE_REG_SECTOR = addr;

			IDE_REG_HEAD = REG_HEAD_DEFAULT | REG_HEAD_LBA;
			 
		} else {

			IDE_REG_SECTOR = addr % disk_sects + 1;
			sector = addr / disk_sects;
			IDE_REG_HEAD = sector % disk_heads | REG_HEAD_DEFAULT;
			sector /= disk_heads;
			IDE_REG_CYL_HI = sector >> 8;
			IDE_REG_CYL_LO = sector;
		}

		nsect = count > 255 ? 255 : count;
		IDE_REG_NSECT = nsect;

		if(ata_read(cmnd, data, nsect, TIMEOUT_READ))
			return -1;

		addr += 255;
		data += 255 * 512;
		count -= nsect;
	}

	return 0;
}

/*
 * initialise IDE interface
 */
void ide_init(void)
{
	/* enable VIA IDE I/O access */

	pcicfg_write_half(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x04, 0x0001 |
		pcicfg_read_half(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x40));

	/* enable primary channel */

	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x40, 0x02 |
		pcicfg_read_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x40));

	ide_reset();
}

/*
 * return handle to disk partition
 */
void *ide_open(const char *name)
{
	struct part_table
	{
		uint8_t				padding[512 - sizeof(uint16_t) - 4 * sizeof(struct part_entry)];
		struct part_entry	p[4];
		uint16_t				signature;

	} __attribute__((packed));

	static struct part_table table;
	unsigned long part;
	unsigned indx;
	char *ptr;

	assert(sizeof(table) == 512);

	part = 0;

	if(name) {
		part = evaluate(name, &ptr);
		if(*ptr || !part || part > 4) {
			puts("invalid device");
			return NULL;
		}
	}

	if((flags & (FLAG_RESETTING | FLAG_IDENTIFIED)) != FLAG_IDENTIFIED) {
		ide_identify();
		if((flags & (FLAG_RESETTING | FLAG_IDENTIFIED)) != FLAG_IDENTIFIED) {
			ide_reset();
			return NULL;
		}
	}

	if(ide_read_sectors(NULL, &table, 0, 1) < 0) {
		ide_reset();
		return NULL;
	}

	if(table.signature != 0xaa55) {
		puts("invalid partition table");
		return NULL;
	}

	if(part) {
	
		if(table.p[part - 1].type != 0x83) {
			puts("not an EXT2 partition");
			return NULL;
		}

	} else {

		/* find bootable ext2 partition, failing that find first ext2 partition */

		for(indx = 0; indx < 4; ++indx)

			if(table.p[indx].type == 0x83) {

				if(table.p[indx].boot & 0x80) {
					part = indx + 1;
					break;
				}

				if(!part)
					part = indx + 1;
			}

		if(!part) {
			puts("no EXT2 partitions");
			return NULL;
		}

		DPRINTF("ide: partition %lu\n", part);
	}

	return &table.p[part - 1];
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
