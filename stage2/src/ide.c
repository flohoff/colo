/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "pci.h"
#include "galileo.h"

#define PIO_MODE_DEFAULT			0

#define TIMEOUT_RESET				(30 * 100)
#define TIMEOUT_IDENTIFY			(2 * 100)
#define TIMEOUT_ATA_READ			(5 * 100)
#define TIMEOUT_ATAPI_READ			(10 * 100)
#define TIMEOUT_PACKET				(1 * 100)
#define TIMEOUT_REQUEST_SENSE		(1 * 100)

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
#define ATA_PACKET					0xa0
#define ATA_ATAPI_IDENTIFY			0xa1
#define ATA_IDENTIFY					0xec
#define ATAPI_REQUEST_SENSE		0x03
#define ATAPI_READ_10				0x28

#define ATA_READ_BLOCK				64
#define ATAPI_READ_BLOCK			64

#define FLAG_RESETTING				(1 << 0)
#define FLAG_IDENTIFIED				(1 << 1)
#define FLAG_LBA						(1 << 2)
#define FLAG_LBA_48					(1 << 3)
#define FLAG_ATAPI					(1 << 4)

#define PART_TYPE_EXT2				0x83
#define PART_TYPE_RAID				0xfd

static struct ide_device
{
	const char		*name;
	unsigned			select;
	unsigned			flags;
	unsigned			mode;

	unsigned long	devsize;
	unsigned			nsects;
	unsigned			ncyls;
	unsigned			nheads;

} ide_bus[2] = {
	{
		.name = "hda"
	}, {
		.name = "hdb",
		.select = REG_HEAD_SLAVE
	}
};

struct part_entry
{
	uint32_t boot:8;
	uint32_t start_chs:24;
	uint32_t type:8;
	uint32_t end_chs:24;
	uint32_t start_lba;
	uint32_t size_lba;

} __attribute__((packed));

static struct
{
	struct ide_device	*dev;
	unsigned long		offset;
	unsigned				block;
	char					ident[8];

} selected;

static unsigned reg_head;

/*
 * select drive
 */
static void ide_select(struct ide_device *dev)
{
	unsigned head;

	head = REG_HEAD_DEFAULT | dev->select;

	if(reg_head != head) {

		reg_head = head;

		IDE_REG_HEAD = reg_head;

		udelay(500);
	}
}

/*
 * reset drives but don't wait
 */
static void ide_reset_async(void)
{
	if(!(ide_bus[0].flags & FLAG_RESETTING)) {

		DPUTS("ide: resetting");

		IDE_REG_CONTROL = REG_CONTROL_DEFAULT | REG_CONTROL_RESET;
		udelay(500);
		IDE_REG_CONTROL = REG_CONTROL_DEFAULT;
		udelay(500);

		ide_bus[0].flags |= FLAG_RESETTING;
		ide_bus[1].flags |= FLAG_RESETTING;

		reg_head = -1;
	}
}

/*
 * reset drives
 */
static int ide_reset(void)
{
	unsigned mark, timeout;
	int drive;

	ide_reset_async();

	ide_bus[0].flags &= ~FLAG_RESETTING;
	ide_bus[1].flags &= ~FLAG_RESETTING;

	timeout = TIMEOUT_RESET;

	drive = 0;

	ide_select(&ide_bus[drive]);

	for(mark = MFC0(CP0_COUNT);;) {

		if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE / 100) {

			if(!(IDE_REG_STATUS & REG_STATUS_BSY)) {

				if(++drive == 2)
					break;

				ide_select(&ide_bus[drive]);

			} else

				if((int) --timeout <= 0) {
					DPUTS("ide: reset timeout");
					return -1;
				}

			mark += CP0_COUNT_RATE / 100;
		}

		if(BREAK()) {
			ide_reset_async();
			return -1;
		}
	}

	return 0;
}

/*
 * issue ATA read command to drive
 */
static int ata_read(struct ide_device *dev, unsigned cmnd, void *data, unsigned count, unsigned timeout)
{
	unsigned stat, expire;
	unsigned long mark;
	void *end;

	assert(!((unsigned long) data & 1));
	
	if(IDE_REG_STATUS & (REG_STATUS_BSY | REG_STATUS_DRQ)) {
		printf("ide: 0x%02x drive busy\n", cmnd);
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

					if(cmnd != ATA_IDENTIFY)
						printf("ide: 0x%02x error 0x%02x\n", cmnd, IDE_REG_ERROR);

					return -1;
				}

				if(!count)
					return 0;

				if(stat & REG_STATUS_DRQ)
					break;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE / 100) {

				if((int) --expire <= 0) {
					printf("ide: 0x%02x command timeout\n", cmnd);
					return -1;
				}

				mark += CP0_COUNT_RATE / 100;
			}
		}

		for(end = data + 512; data < end; data += 16) {

			((uint16_t *) data)[0] = IDE_REG_DATA;
			((uint16_t *) data)[1] = IDE_REG_DATA;
			((uint16_t *) data)[2] = IDE_REG_DATA;
			((uint16_t *) data)[3] = IDE_REG_DATA;
			((uint16_t *) data)[4] = IDE_REG_DATA;
			((uint16_t *) data)[5] = IDE_REG_DATA;
			((uint16_t *) data)[6] = IDE_REG_DATA;
			((uint16_t *) data)[7] = IDE_REG_DATA;
		}

		--count;

		IDE_REG_STATUS_ALT;
	}
}

/*
 * extract model name from identify information
 */
static const char *ide_model(const void *info)
{
	static char model[(47 - 27) * 2 + 1];
	unsigned indx;

	for(indx = 0; indx < (47 - 27) * 2; ++indx)
		model[indx] = ((char *) info)[27 * 2 + (indx ^ 1)];
	while(indx && isspace(model[indx - 1]))
		--indx;
	model[indx] = '\0';

	return model;
}

/*
 * establish PIO mode from identify information
 */
static unsigned ide_identify_mode(const void *info)
{
	unsigned timing, mode;
	union {
		const void	*info;
		uint8_t		*b;
		uint16_t		*h;
		uint32_t		*w;
	} data;

	data.info = info;

	mode = PIO_MODE_DEFAULT;

	if(data.h[53] & (1 << 1)) {
		timing = data.h[64];
		if(timing & (1 << 1))
			mode = 4;
		else if(timing & (1 << 0))
			mode = 3;
	}

	DPRINTF("ide: supports PIO mode %u\n", mode);

	return mode;
}

/*
 * parse ATAPI identify information
 */
static int ide_atapi_identify(struct ide_device *dev, const void *info)
{
	union {
		const void	*info;
		uint8_t		*b;
		uint16_t		*h;
		uint32_t		*w;
	} data;

	data.info = info;

	dev->flags = 0;

	if((data.h[0] & 0xc000) != 0x8000) {
		puts("ide: not ATAPI device");
		return -1;
	}

	if((data.h[0] & 0x1f00) != 0x0500) {
		puts("ide: not a CDROM drive");
		return -1;
	}

	if((data.h[0] & 3) != 0) {
		puts("ide: unsupported ATAPI packet size");
		return -1;
	}

#ifdef _DEBUG
	putstring("ide: {");
	putstring_safe(ide_model(info), -1);
	puts("}");
#endif

	dev->mode = ide_identify_mode(info);

	dev->flags |= FLAG_IDENTIFIED | FLAG_ATAPI;

	return 0;
}

/*
 * parse ATA identify information
 */
static int ide_ata_identify(struct ide_device *dev, const void *info)
{
	union {
		const void	*info;
		uint8_t		*b;
		uint16_t		*h;
		uint32_t		*w;
	} data;

	data.info = info;

	dev->flags = 0;
	dev->select &= REG_HEAD_SLAVE;

	if(data.h[0] & (1 << 15)) {
		puts("ide: not ATA device");
		return -1;
	}

#ifdef _DEBUG
	putstring("ide: {");
	putstring_safe(ide_model(info), -1);
	puts("}");
#endif

	if((data.h[49] & (1 << 9)) &&
		!(nv_store.flags & NVFLAG_IDE_DISABLE_LBA))
	{
		dev->flags |= FLAG_LBA;
		if((data.h[83] & ((1 << 15) | (1 << 14) | (1 << 10))) == ((1 << 14) | (1 << 10)) &&
			!(nv_store.flags & NVFLAG_IDE_DISABLE_LBA48))
		{
			dev->flags |= FLAG_LBA_48;
			if(data.w[102 / 2]) {
				dev->devsize = ~0;
				puts("disk size capped at 2TB!");
			} else
				dev->devsize = data.w[100 / 2];
			DPRINTF("ide: LBA48 %lu\n", dev->devsize);
		} else {
			dev->devsize = data.w[60 / 2];
			DPRINTF("ide: LBA %lu\n", dev->devsize);
		}
		dev->select |= REG_HEAD_LBA;
	}

	if(!(dev->flags & FLAG_LBA)) {
		dev->ncyls = data.h[54];
		dev->nheads = data.h[55];
		dev->nsects = data.h[56];
		dev->devsize = (unsigned long) dev->ncyls * dev->nheads * dev->nsects;
		if(!dev->devsize || dev->nheads > 16 || dev->ncyls > 65536 || dev->nsects >= 256) {
			puts("ide: invalid CHS values");
			return -1;
		}
		DPRINTF("ide: CHS %u/%u/%u (%lu)\n", dev->ncyls, dev->nheads, dev->nsects, dev->devsize);
	}

	dev->mode = ide_identify_mode(info);

	dev->flags |= FLAG_IDENTIFIED;

	return 0;
}

/*
 * identify drive
 */
static int ide_identify(struct ide_device *dev)
{
	static uint8_t data[512];

	ide_select(dev);

	dev->flags = 0;

	IDE_REG_CYL_LO = 0x55;
	IDE_REG_CYL_HI = 0xaa;

	if(IDE_REG_CYL_LO == 0x55 ||
		IDE_REG_CYL_HI == 0xaa) {

		if(!ata_read(dev, ATA_IDENTIFY, data, 1, TIMEOUT_IDENTIFY)) 
			return ide_ata_identify(dev, data);

		if(IDE_REG_CYL_LO == 0x14 &&
			IDE_REG_CYL_HI == 0xeb &&
			!ata_read(dev, ATA_ATAPI_IDENTIFY, data, 1, TIMEOUT_IDENTIFY)) {

			return ide_atapi_identify(dev, data);
		}
	}

	puts("ide: no drive found");

	return -1;
}

/*
 * issue ATAPI read command to drive
 */
static int atapi_read(struct ide_device *dev, const void *cmnd, void *data, unsigned blksz, unsigned count, unsigned timeout)
{
	unsigned stat, expire, indx;
	unsigned long mark;

	assert((dev->flags & FLAG_IDENTIFIED) && (dev->flags & FLAG_ATAPI));
	assert(!((unsigned long) data & 1));
	assert(!((unsigned long) cmnd & 1));
	assert(!(blksz & 1));
	
	if(IDE_REG_STATUS & (REG_STATUS_BSY | REG_STATUS_DRQ)) {
		printf("ide: 0x%04x drive busy\n", ((uint16_t *) cmnd)[0]);
		return -1;
	}

	IDE_REG_CYL_LO = blksz;
	IDE_REG_CYL_HI = blksz >> 8;

	IDE_REG_COMMAND = ATA_PACKET;
	udelay(1);

	expire = TIMEOUT_PACKET;

	for(mark = MFC0(CP0_COUNT);;) {

		stat = IDE_REG_STATUS;

		if(!(stat & REG_STATUS_BSY)) {

			if(stat & REG_STATUS_ERR) {

				printf("ide: packet error 0x%04x 0x%02x\n", ((uint16_t *) cmnd)[0], IDE_REG_ERROR);

				return -1;
			}

			if(stat & REG_STATUS_DRQ)
				break;
		}

		if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE / 100) {

			if((int) --expire <= 0) {
				printf("ide: packet timeout 0x%04x\n", ((uint16_t *) cmnd)[0]);
				return -1;
			}

			mark += CP0_COUNT_RATE / 100;
		}
	}

	IDE_REG_DATA = ((uint16_t *) cmnd)[0];
	IDE_REG_DATA = ((uint16_t *) cmnd)[1];
	IDE_REG_DATA = ((uint16_t *) cmnd)[2];
	IDE_REG_DATA = ((uint16_t *) cmnd)[3];
	IDE_REG_DATA = ((uint16_t *) cmnd)[4];
	IDE_REG_DATA = ((uint16_t *) cmnd)[5];

	udelay(5 * 1000);

	for(blksz >>= 1;;) {

		IDE_REG_STATUS_ALT;

		expire = timeout;

		for(mark = MFC0(CP0_COUNT);;) {

			stat = IDE_REG_STATUS;

			if(!(stat & REG_STATUS_BSY)) {

				if(stat & REG_STATUS_ERR) {

					printf("ide: command error 0x%04x\n", ((uint16_t *) cmnd)[0]);

					return -1;
				}

				if(!count)
					return 0;

				if(stat & REG_STATUS_DRQ)
					break;
			}

			if(MFC0(CP0_COUNT) - mark >= CP0_COUNT_RATE / 100) {

				if((int) --expire <= 0) {
					printf("ide: command timeout 0x%04x\n", ((uint16_t *) cmnd)[0]);
					return -1;
				}

				mark += CP0_COUNT_RATE / 100;
			}
		}

		for(indx = 0; indx < blksz; ++indx)
			((uint16_t *) data)[indx] = IDE_REG_DATA;

		--count;
	}
}

/*
 * read sectors from drive
 */
int ata_read_sectors(struct ide_device *dev, void *data, unsigned long addr, unsigned count)
{
	unsigned long sector;
	unsigned cmnd, nsect;

	assert(dev->flags & FLAG_IDENTIFIED);

	if(addr + count >= dev->devsize) {
		puts("ide: attempt to read past end of disk");
		return -1;
	}

	ide_select(dev);

	for(cmnd = ATA_READ; count;) {

		if(dev->flags & FLAG_LBA) {

			assert(reg_head & REG_HEAD_LBA);

			if(dev->flags & FLAG_LBA_48) {

				cmnd = ATA_READ_EXT;

				IDE_REG_CYL_HI = 0;
				IDE_REG_CYL_LO = 0;
				IDE_REG_SECTOR = addr >> 24;

				IDE_REG_NSECT = 0;

			} else

				IDE_REG_HEAD = reg_head | (addr >> 24);

			IDE_REG_CYL_HI = addr >> 16;
			IDE_REG_CYL_LO = addr >> 8;
			IDE_REG_SECTOR = addr;

		} else {

			assert(!(reg_head & REG_HEAD_LBA));

			IDE_REG_SECTOR = addr % dev->nsects + 1;
			sector = addr / dev->nsects;
			IDE_REG_HEAD = reg_head | sector % dev->nheads;
			sector /= dev->nheads;
			IDE_REG_CYL_HI = sector >> 8;
			IDE_REG_CYL_LO = sector;
		}

		nsect = count > ATA_READ_BLOCK ? ATA_READ_BLOCK : count;
		IDE_REG_NSECT = nsect;

		if(ata_read(dev, cmnd, data, nsect, TIMEOUT_ATA_READ))
			return -1;

		addr += ATA_READ_BLOCK;
		data += ATA_READ_BLOCK * 512;
		count -= nsect;
	}

	return 0;
}

/*
 * request sense from ATAPI device
 */
static int atapi_sense(struct ide_device *dev)
{
	union {
		uint16_t	h[6];
		uint8_t	b[1];
	} cmnd;
	union {
		uint16_t	h[9];
		uint8_t	b[1];
	} sense;

	assert((dev->flags & FLAG_IDENTIFIED) && (dev->flags & FLAG_ATAPI));

	cmnd.b[0]	= ATAPI_REQUEST_SENSE;
	cmnd.b[1]	= 0x00;
	cmnd.b[2]	= 0x00;
	cmnd.b[3]	= 0x00;
	cmnd.b[4]	= sizeof(sense);
	cmnd.b[5]	= 0x00;
	cmnd.b[6]	= 0x00;
	cmnd.b[7]	= 0x00;
	cmnd.b[8]	= 0x00;
	cmnd.b[9]	= 0x00;
	cmnd.b[10]	= 0x00;
	cmnd.b[11]	= 0x00;

	if(atapi_read(dev, cmnd.b, sense.b, sizeof(sense), 1, TIMEOUT_REQUEST_SENSE))
		return -1;

	return ((unsigned) sense.b[2] << 16) | ((unsigned) sense.b[12] << 8) | sense.b[13];
}

/*
 * read sectors from ATAPI device
 */
int atapi_read_sectors(struct ide_device *dev, void *data, unsigned long addr, unsigned count)
{
	static char emsg[32];
	unsigned work, retry, mark;
	unsigned long end;
	union {
		uint16_t	h[6];
		uint8_t	b[1];
	} cmnd;
	char *cause;
	int sense;

	ide_select(dev);

	cmnd.b[0]	= ATAPI_READ_10;
	cmnd.b[1]	= 0x00;
	cmnd.b[6]	= 0x00;
	cmnd.b[9]	= 0x00;
	cmnd.b[10]	= 0x00;
	cmnd.b[11]	= 0x00;

	for(end = addr + count; addr < end;) {

		work = end - addr;
		if(work > ATAPI_READ_BLOCK)
			work = ATAPI_READ_BLOCK;

		cmnd.b[2] = addr >> 24;
		cmnd.b[3] = addr >> 16;
		cmnd.b[4] = addr >> 8;
		cmnd.b[5] = addr;

		cmnd.b[7] = work >> 8;
		cmnd.b[8] = work;

		for(retry = 1;; ++retry) {

			if(!atapi_read(dev, cmnd.b, data, 2048, work, TIMEOUT_ATAPI_READ))
				break;

			if(retry == 20)
				return -1;

			sense = atapi_sense(dev);

			if(sense < 0) {

				if(ide_reset() < 0)
					return -1;
				ide_select(dev);

			} else {

				cause = emsg;

				switch(sense) {
					case 0x023a00:
						DPRINTF("ide.%s: error {No Medium}\n", dev->name);
						return -1;
					case 0x062800:
						cause = "Media Change";
						break;
					case 0x062900:
						cause = "Reset Complete";
						break;
					case 0x020401:
						cause = "Spinning Up";
						break;
					default:
						sprintf(cause, "#%06x", sense);
				}

				DPRINTF("ide: error {%s}, retry\n", cause);
			}

			for(mark = MFC0(CP0_COUNT); MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE / 2;)
				;
		}

		addr += work;
		data += work * 2048;
	}

	return 0;
}

/*
 * read sectors from drive
 */
int ide_read_sectors(void *device, void *data, unsigned long addr, unsigned count)
{
	assert(device == &selected);

	addr += selected.offset;

	return (selected.dev->flags & FLAG_ATAPI) ?
		atapi_read_sectors(selected.dev, data, addr, count) :
		ata_read_sectors(selected.dev, data, addr, count);
}

/*
 * get drive sector size
 */
int ide_block_size(void *device)
{
	assert(device == &selected);

	return selected.block;
}

/*
 * get partition identification
 */
const char *ide_dev_name(void *device)
{
	assert(device == &selected);

	return selected.ident;
}

/*
 * set timing for PIO mode
 */
static void ide_timing(unsigned mode)
{
	if(mode != 4)
		return;

	DPUTS("ide: mode 4 timing");

#	define PCI_CLOCKS(t)			((unsigned)((1LL*PCI_CLOCK*(t)+999999999)/1000000000))

#	define NCLKS_SETUP			PCI_CLOCKS(25)
#	define NCLKS_ACTIVE			PCI_CLOCKS(70)
#	define NCLKS_RECOVER			PCI_CLOCKS(25)

	/* set port timing */

	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x4b, ((NCLKS_ACTIVE - 1) << 4) | (NCLKS_RECOVER - 1));
	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x4c, ((NCLKS_SETUP - 1) << 6) | 0x3f);
	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x4f, ((NCLKS_ACTIVE - 1) << 4) | (NCLKS_RECOVER - 1));

	/* enable prefetch buffer */

	pcicfg_write_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x41, 0x80 |
		pcicfg_read_byte(PCI_DEV_VIA, PCI_FNC_VIA_IDE, 0x41));
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

	ide_timing(PIO_MODE_DEFAULT);

	ide_reset_async();
}

/*
 * return handle to drive/partition
 */
void *ide_open(const char *name)
{
	struct part_table
	{
		uint8_t				padding[512 - sizeof(uint16_t) - 4 * sizeof(struct part_entry)];
		struct part_entry	p[4];
		uint16_t				signature;

	} __attribute__((packed));

	static const char *prefix[] = { "/dev/hd", "hd" };
	static struct part_table table;
	int disk, cdrom, drive, part;
	unsigned indx, size, mode;
	char *ptr;

	assert(sizeof(table) == 512);

	if(!((ide_bus[0].flags | ide_bus[1].flags) & FLAG_IDENTIFIED)) {

		if(ide_reset() < 0) {
			puts("aborted");
			return NULL;
		}

		ide_identify(&ide_bus[0]);
		if(nv_store.flags & NVFLAG_IDE_ENABLE_SLAVE)
			ide_identify(&ide_bus[1]);

		if(!((ide_bus[0].flags | ide_bus[1].flags) & FLAG_IDENTIFIED)) {
			puts("no devices found");
			return NULL;
		}

		mode = 10;
		for(indx = 0; indx < 2; ++indx)
			if((ide_bus[indx].flags & FLAG_IDENTIFIED) && ide_bus[indx].mode < mode)
					mode = ide_bus[indx].mode;

		if(!(nv_store.flags & NVFLAG_IDE_DISABLE_TIMING))
			ide_timing(mode);
	}

	disk = -1;
	cdrom = -1;

	for(indx = 0; indx < 2; ++indx)
		if(ide_bus[indx].flags & FLAG_IDENTIFIED) {
			if(ide_bus[indx].flags & FLAG_ATAPI) {
				if(cdrom < 0)
					cdrom = indx;
			} else if(disk < 0)
				disk = indx;
		}

	drive = disk < 0 ? cdrom : disk;
	part = -1;

	if(name) {

		size = strlen(name);

		if(!strncasecmp(name, "disk", size))
			
			drive = disk;

		else if(!strncasecmp(name, "cdrom", size))

			drive = cdrom;

		else {

			drive = -1;

			for(indx = 0; indx < elements(prefix); ++indx) {
				size = strlen(prefix[indx]);
				if(!strncasecmp(name, prefix[indx], size)) {

					if(toupper(name[size]) == 'A' || toupper(name[size]) == 'B') {

						part = 0;

						ptr = (char *) &name[size + 1];
						if(*ptr)
							part = strtoul(ptr, &ptr, 10);

						if(!*ptr)
							drive = (name[size] - 'A') & 1;
					}

					break;
				}
			}

			if(drive < 0) {
				puts("invalid device specification");
				return NULL;
			}
		}
	}

	if(drive < 0 || !(ide_bus[drive].flags & FLAG_IDENTIFIED)) {
		puts("no such device");
		return NULL;
	}

	selected.dev = &ide_bus[drive];
	selected.offset = 0;
	selected.block = (ide_bus[drive].flags & FLAG_ATAPI) ? 2048 : 512;

	if(!part) {
		sprintf(selected.ident, "hd%c", drive + 'a');
		return &selected;
	}

	if(selected.dev->flags & FLAG_ATAPI) {
		if(part < 0)
			return &selected;
		puts("no partition support for CDROMs");
		return NULL;
	}

	if(part > elements(table.p)) {
		puts("logical partitions not supported");
		return NULL;
	}

	if(ide_read_sectors(&selected, &table, 0, 1) < 0) {
		ide_reset();
		return NULL;
	}

	if(table.signature != 0xaa55) {
		puts("invalid partition table");
		return NULL;
	}

	if(part > 0) {
	
		switch(table.p[--part].type) {

			case PART_TYPE_EXT2:
			case PART_TYPE_RAID:
				break;

			default:
				puts("not an EXT2/EXT3/RAID partition");
				return NULL;
		}

	} else {

		/* find bootable partition, failing that find first partition we recognise */

		for(indx = 0; indx < elements(table.p); ++indx)

			switch(table.p[indx].type) {

				case PART_TYPE_EXT2:
				case PART_TYPE_RAID:

					if(table.p[indx].boot & 0x80) {
						part = indx;
						indx = elements(table.p);
						break;
					}

					if(part < 0)
						part = indx;
			}

		if(part < 0) {
			puts("no EXT2/EXT3/RAID partitions");
			return NULL;
		}

		DPRINTF("ide: partition %d\n", part + 1);
	}

	selected.offset = table.p[part].start_lba;

	sprintf(selected.ident, "hd%c%d", drive + 'a', part + 1);

	return &selected;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
