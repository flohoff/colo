/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "net.h"
#include "pci.h"
#include "cpu.h"
#include "galileo.h"

#define IO_BASE_ETH0						0x10100000
#define IO_BASE_ETH1						0x10101000

#define TULIP_VND_ID						0x1011
#define TULIP_DEV_ID						0x0019

#define RX_RING_SIZE						4
#define TX_RING_SIZE						4

#define _RX_BUFFER_SIZE					(sizeof(((struct frame *) 0)->payload))

#define CSR(n)								(((volatile unsigned *) KSEG1(IO_BASE_ETH0))[(n)*2])

#define CSR0_SWR							(1 << 0)
#define CSR0_PBL							(0 << 8)			/* unlimited burst length */
#define CSR0_CAL							(3 << 14)		/* 32 byte cache line     */
#define CSR0_RME							(1 << 21)
#define CSR0_RLE							(1 << 23)
#define CSR0_WIE							(1 << 24)

#define CSR6_SR							(1 << 1)
#define CSR6_ST							(1 << 13)
#define CSR6_PS							(1 << 18)
#define CSR6_HBD							(1 << 19)
#define CSR6_MBO							(1 << 25)

#define CSR9_SROM_CS						(1 << 0)
#define CSR9_SROM_CK						(1 << 1)
#define CSR9_SROM_DI						(1 << 2)
#define CSR9_SROM_DO						(1 << 3)
#define CSR9_SR							(1 << 11)
#define CSR9_RD							(1 << 14)

#define RX_DESC_STATUS_LS				(1 << 8)
#define RX_DESC_STATUS_FS				(1 << 9)
#define RX_DESC_STATUS_ES				(1 << 15)
#define RX_DESC_STATUS_FL(x)			(((x) >> 16) & 0x3fff)
#define RX_DESC_STATUS_OWN				(1 << 31)
#define RX_DESC_LENGTH_RER				(1 << 25)

#define TX_DESC_STATUS_OWN				(1 << 31)
#define TX_DESC_LENGTH_TER				(1 << 25)
#define TX_DESC_LENGTH_SET				(1 << 27)
#define TX_DESC_LENGTH_FS				(1 << 29)
#define TX_DESC_LENGTH_LS				(1 << 30)

#define ADDR_FILT_SIZE					192

#define RX_BUFFER_SIZE					((_RX_BUFFER_SIZE&(DCACHE_LINE_SIZE-1))?_RX_BUFFER_SIZE:_RX_BUFFER_SIZE-4)

struct descriptor
{
	unsigned	status;
	unsigned	length;
	unsigned	buffer1;
	unsigned	buffer2;
};

uint16_t hw_addr[3];

static struct frame *tx_frame[TX_RING_SIZE];
static struct descriptor *tx_desc;
static unsigned tx_next;
static unsigned tx_curr;

static struct frame *rx_frame[RX_RING_SIZE];
static struct descriptor *rx_desc;
static unsigned rx_curr;
static unsigned rx_fill;

static int nic_avail;

/*
 * refill receive ring
 */
static void rx_ring_fill(void)
{
	unsigned curr;

	while(rx_fill - rx_curr < RX_RING_SIZE) {

		curr = rx_fill % RX_RING_SIZE;

		rx_frame[curr] = frame_alloc();
		if(!rx_frame[curr])
			break;

		dcache_flush((unsigned long) rx_frame[curr]->payload, RX_BUFFER_SIZE);

		rx_desc[curr].buffer1 = (unsigned long) KPHYS(rx_frame[curr]->payload);
		rx_desc[curr].status = RX_DESC_STATUS_OWN;
		CSR(2) = 0;

		++rx_fill;
	}
}

/*
 * initialise receive ring
 */
static void rx_ring_init(void)
{
	static struct descriptor ring[RX_RING_SIZE];
	unsigned indx;

	rx_desc = KSEG1(ring);
	rx_curr = 0;
	rx_fill = 0;

	for(indx = 0; indx < RX_RING_SIZE; ++indx) {

		rx_desc[indx].status = 0;
		rx_desc[indx].length = RX_BUFFER_SIZE;
	}

	rx_desc[RX_RING_SIZE - 1].length = RX_DESC_LENGTH_RER | RX_BUFFER_SIZE;

	CSR(3) = (unsigned long) KPHYS(rx_desc);

	rx_ring_fill();
}

/*
 * initialise transmit ring
 */
static void tx_ring_init(void)
{
	static struct descriptor ring[TX_RING_SIZE];
	unsigned indx;

	tx_desc = KSEG1(ring);
	tx_curr = 0;
	tx_next = 0;

	for(indx = 0; indx < TX_RING_SIZE; ++indx)
		tx_desc[indx].status = 0;

	CSR(4) = (unsigned long) KPHYS(tx_desc);
}

/*
 * initialis receive filter
 */
static void rx_filter_init(void)
{
	static unsigned filt[ADDR_FILT_SIZE / sizeof(unsigned)];
	unsigned indx, curr, size;

	assert(tx_next - tx_curr < TX_RING_SIZE);

	for(indx = 0; indx < elements(filt); ++indx)
		filt[indx] = (indx < elements(hw_addr) ? hw_addr[indx] : 0xffff);

	dcache_flush((unsigned long) filt, sizeof(filt));

	size = sizeof(filt);

	curr = tx_next++ % TX_RING_SIZE;
	if(curr == TX_RING_SIZE - 1)
		size |= TX_DESC_LENGTH_TER;

	tx_frame[curr] = NULL;
	tx_desc[curr].buffer1 = (unsigned long) KPHYS(filt);
	tx_desc[curr].length = TX_DESC_LENGTH_SET | size;
	tx_desc[curr].status = TX_DESC_STATUS_OWN;
	CSR(1) = 0;
}

/*
 * poll receive and transmit ring
 */
void tulip_poll(void)
{
	unsigned curr, stat, size;
	struct frame *frame;

	assert(net_is_up());

	while(rx_curr != rx_fill) {

		curr = rx_curr % RX_RING_SIZE;

		stat = rx_desc[curr].status;
		if(stat & RX_DESC_STATUS_OWN)
			break;

		++rx_curr;

		size = RX_DESC_STATUS_FL(stat) - 4;
		stat &= RX_DESC_STATUS_ES | RX_DESC_STATUS_FS | RX_DESC_STATUS_LS;
		if(stat == (RX_DESC_STATUS_FS | RX_DESC_STATUS_LS) &&
			size <= sizeof(rx_frame[curr]->payload)) {

			frame = rx_frame[curr];
			FRAME_INIT(frame, 0, size);

			rx_ring_fill();

			net_in(frame);

		} else {

			rx_desc[curr].status = RX_DESC_STATUS_OWN;
			CSR(2) = 0;

			++rx_fill;
		}
	}

	while(tx_curr != tx_next) {

		curr = tx_curr % TX_RING_SIZE;

		if(tx_desc[curr].status & TX_DESC_STATUS_OWN)
			break;

		if(tx_frame[curr])
			frame_free(tx_frame[curr]);

		++tx_curr;
	}

	rx_ring_fill();
}

/*
 * queue frame for transmit
 */
void tulip_out(struct frame *frame)
{
	unsigned size, curr;
	void *data;

	assert(net_is_up());

	if(tx_next - tx_curr >= TX_RING_SIZE) {
		frame_free(frame);
		return;
	}

	data = FRAME_PAYLOAD(frame);
	size = FRAME_SIZE(frame);

	dcache_flush((unsigned long) data, size);

	if(size < HARDWARE_MIN_FRAME_SZ - 4)
		size = HARDWARE_MIN_FRAME_SZ - 4;

	curr = tx_next++ % TX_RING_SIZE;
	if(curr == TX_RING_SIZE - 1)
		size |= TX_DESC_LENGTH_TER;

	tx_frame[curr] = frame;
	tx_desc[curr].buffer1 = (unsigned long) KPHYS(data);
	tx_desc[curr].length = TX_DESC_LENGTH_FS | TX_DESC_LENGTH_LS | size;
	tx_desc[curr].status = TX_DESC_STATUS_OWN;
	CSR(1) = 0;
}

/*
 * read 16-bits from EEPROM
 */
static unsigned eeprom_read(unsigned addr)
{
	unsigned indx, data;

	CSR(9) = CSR9_RD | CSR9_SR;
	udelay(5);
	CSR(9) = CSR9_RD | CSR9_SR | CSR9_SROM_CS;
	udelay(5);

	addr |= 6 << 6;

	for(indx = 0; indx < 1 + 2 + 6; ++indx) {

		data = CSR9_RD | CSR9_SR | CSR9_SROM_CS;
		if(addr & (1 << (2 + 6)))
			data |= CSR9_SROM_DI;

		CSR(9) = data;
		udelay(5);
		CSR(9) = data | CSR9_SROM_CK;
		udelay(5);

		addr <<= 1;
	}

	data = 0;

	for(indx = 0; indx < 16; ++indx) {

		CSR(9) = CSR9_RD | CSR9_SR | CSR9_SROM_CS;
		udelay(5);
		CSR(9) = CSR9_RD | CSR9_SR | CSR9_SROM_CK | CSR9_SROM_CS;
		udelay(5);

		data <<= 1;
		if(CSR(9) & CSR9_SROM_DO)
			data |= 1;
	}

	CSR(9) = CSR9_RD | CSR9_SR;

	return data;
}

/*
 * read hardware address from EEPROM
 */
static void read_hw_addr(void)
{
	hw_addr[0] = eeprom_read(0);
	hw_addr[1] = eeprom_read(1);
	hw_addr[2] = eeprom_read(2);

#ifdef _DEBUG
	{
		static char buf[24];
		unsigned indx;

		for(indx = 0; indx < 6; ++indx)
			sprintf(buf + indx * 3, "%02x:", ((uint8_t *) hw_addr)[indx]);
		buf[17] = '\0';

		DPRINTF("tulip: {%s}\n", buf);
	}
#endif
}

/*
 * set up PCI I/O mapping etc
 */
static int tulip_setup(unsigned dev, unsigned fnc, unsigned iob)
{
	if(pcicfg_read_word(dev, fnc, 0x00) != ((TULIP_DEV_ID << 16) | TULIP_VND_ID))
		return 0;

	pcicfg_write_word(dev, fnc, 0x10, iob);

	pcicfg_write_half(dev, fnc, 0x04,
		pcicfg_read_half(dev, fnc, 0x04) | (1 << 0));

	pcicfg_write_byte(dev, fnc, 0x0c, DCACHE_LINE_SIZE / 4);

	pcicfg_write_byte(dev, fnc, 0x0d, 32);

	return 1;
}

static void tulip_reset(void)
{
	CSR(0) = CSR0_SWR;
	udelay(1000);

	CSR(0) = 0;
	udelay(1000);

	CSR(6) = CSR6_MBO | CSR6_HBD | CSR6_PS;
	udelay(1000);
}

void tulip_init(void)
{
	nic_avail = tulip_setup(PCI_DEV_ETH0, PCI_FNC_ETH0, IO_BASE_ETH0);
	tulip_setup(PCI_DEV_ETH1, PCI_FNC_ETH1, IO_BASE_ETH1);

	/* without this Tulip bus mastering doesn't work correctly */

	BRDG_REG_WORD(BRDG_REG_TIMEOUT_RETRY) = 0xffff;

	if(!nic_avail)
		return;

	/* wake up device */

	pcicfg_write_word(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x40, 0x00000000);
	udelay(1000);

	tulip_reset();

	read_hw_addr();
}

int tulip_up(void)
{
	assert(!net_is_up());

	if(!nic_avail)
		return 0;

	tulip_reset();

	rx_ring_init();
	tx_ring_init();

	pcicfg_write_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04,
		pcicfg_read_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04) | (1 << 2));
	udelay(1000);

	CSR(6) = CSR6_MBO | CSR6_HBD | CSR6_PS | CSR6_ST | CSR6_SR;

	rx_filter_init();

	return 1;
}

void tulip_down(void)
{
	assert(net_is_up());

	tulip_reset();

	pcicfg_write_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04,
		pcicfg_read_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04) & ~(1 << 2));
	udelay(1000);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
