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
#define TULIP_DEV_ID_21041				0x0014
#define TULIP_DEV_ID_21143				0x0019

#define CHIP_ID_21041					0
#define CHIP_ID_21143					1

#define PHY_ID								1

#define PHY_REG_CTRL						0
# define PHY_REG_CTRL_RESET			(1 << 15)
#define PHY_REG_CSTAT					20
# define PHY_REG_CSTAT_100MBPS		(1 << 11)
# define PHY_REG_CSTAT_DUPLEX			(1 << 12)
# define PHY_REG_CSTAT_LINK			(1 << 13)

#define LINK_WAIT							5

#define RX_RING_SIZE						4
#define TX_RING_SIZE						4

#define _RX_BUFFER_SIZE					(sizeof(((struct frame *) 0)->payload))

#define CSR(n)								(((volatile unsigned *) KSEG1(IO_BASE_ETH0))[(n)*2])

#define CSR0_SWR							(1 << 0)
#define CSR0_PBL							(0 << 8)			/* unlimited burst length */
#define CSR0_CAL							(3 << 14)		/* 32 byte cache line     */
#define CSR0_21143_RME					(1 << 21)
#define CSR0_21143_RLE					(1 << 23)
#define CSR0_21143_WIE					(1 << 24)

#define CSR5_RS_MASK						(7 << 17)
# define CSR5_RS_STOPPED				(0 << 17)
#define CSR5_TS_MASK						(7 << 20)
# define CSR5_TS_STOPPED				(0 << 20)

#define CSR6_SR							(1 << 1)
#define CSR6_FD							(1 << 9)
#define CSR6_ST							(1 << 13)
#define CSR6_21143_PS					(1 << 18)
#define CSR6_21143_HBD					(1 << 19)
#define CSR6_21143_MBO					(1 << 25)

#define CSR9_SROM_CS						(1 << 0)
#define CSR9_SROM_CK						(1 << 1)
#define CSR9_SROM_DI						(1 << 2)
#define CSR9_SROM_DO						(1 << 3)
#define CSR9_SR							(1 << 11)
#define CSR9_RD							(1 << 14)
#define CSR9_MDC							(1 << 16)
#define CSR9_MDO							(1 << 17)
#define CSR9_MII							(1 << 18)
#define CSR9_MDI							(1 << 19)

#define CSR12_21041_LKF					(1 << 2)
#define CSR12_ANS_MASK					(7 << 12)
# define CSR12_ANS_COMPLETE			(5 << 12)
#define CSR12_LPN							(1 << 15)
#define CSR12_LPC_10FDX					(0x10000 << 6)

#define CSR13_SRL							(1 << 0)
#define CSR13_SDM							(0xef0 << 4)

#define CSR14_ECEN						(1 << 0)
#define CSR14_LBK							(1 << 1)
#define CSR14_DREN						(1 << 2)
#define CSR14_LSE							(1 << 3)
#define CSR14_CPEN_NORMAL				(3 << 4)
#define CSR14_MBO							(1 << 6)
#define CSR14_ANE							(1 << 7)
#define CSR14_RSQ							(1 << 8)
#define CSR14_CSQ							(1 << 9)
#define CSR14_CLD							(1 << 10)
#define CSR14_SQE							(1 << 11)
#define CSR14_LTE							(1 << 12)
#define CSR14_APE							(1 << 13)
#define CSR14_SPP							(1 << 14)

#define CSR15_ABM							(1 << 3)

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

#define bit_bang_delay()				do{CSR(5);udelay(2);}while(0)

#define PHY_UNINIT						0
#define PHY_LINK_DOWN					1
#define PHY_LINK_UP						2

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

static unsigned reg_csr6;
static int nic_avail;
static int phy_state;
static int chip_id;

/*
 * read MII PHY register
 */
static unsigned phy_read_mii(unsigned phy, unsigned reg)
{
	unsigned data;
	int indx;

	reg |= (6 << 10) | (phy << 5);

	for(indx = 0; indx < 32; ++indx) {

		CSR(9) = CSR9_MDO;
		bit_bang_delay();

		CSR(9) = CSR9_MDO | CSR9_MDC;
		bit_bang_delay();
	}

	for(indx = 0; indx < 14; ++indx) {

		data = 0;
		if(reg & (1 << 13))
			data = CSR9_MDO;
		reg <<= 1;

		CSR(9) = data;
		bit_bang_delay();

		CSR(9) = data | CSR9_MDC;
		bit_bang_delay();
	}

	for(indx = 0; indx < 18; ++indx) {

		CSR(9) = CSR9_MII;
		bit_bang_delay();

		CSR(9) = CSR9_MII | CSR9_MDC;
		bit_bang_delay();

		data <<= 1;
		if(CSR(9) & CSR9_MDI)
			data |= 1;
	}

	CSR(9) = 0;

	return (data >> 1) & 0xffff;
}

/*
 * write MII PHY register
 */
static void phy_write_mii(unsigned phy, unsigned reg, unsigned value)
{
	unsigned data;
	int indx;

	reg = (5 << 12) | (phy << 7) | (reg << 2) | 2;

	for(indx = 0; indx < 32; ++indx) {

		CSR(9) = CSR9_MDO;
		bit_bang_delay();

		CSR(9) = CSR9_MDO | CSR9_MDC;
		bit_bang_delay();
	}

	for(indx = 0; indx < 16; ++indx) {

		data = 0;
		if(reg & (1 << 15))
			data = CSR9_MDO;
		reg <<= 1;

		CSR(9) = data;
		bit_bang_delay();

		CSR(9) = data | CSR9_MDC;
		bit_bang_delay();
	}

	for(indx = 0; indx < 16; ++indx) {

		data = 0;
		if(value & (1 << 15))
			data = CSR9_MDO;
		value <<= 1;

		CSR(9) = data;
		bit_bang_delay();

		CSR(9) = data | CSR9_MDC;
		bit_bang_delay();
	}

	CSR(9) = 0;
}

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
	bit_bang_delay();
	CSR(9) = CSR9_RD | CSR9_SR | CSR9_SROM_CS;
	bit_bang_delay();

	addr |= 6 << 6;

	for(indx = 0; indx < 1 + 2 + 6; ++indx) {

		data = CSR9_RD | CSR9_SR | CSR9_SROM_CS;
		if(addr & (1 << (2 + 6)))
			data |= CSR9_SROM_DI;

		CSR(9) = data;
		bit_bang_delay();
		CSR(9) = data | CSR9_SROM_CK;
		bit_bang_delay();

		addr <<= 1;
	}

	data = 0;

	for(indx = 0; indx < 16; ++indx) {

		CSR(9) = CSR9_RD | CSR9_SR | CSR9_SROM_CS;
		bit_bang_delay();
		CSR(9) = CSR9_RD | CSR9_SR | CSR9_SROM_CK | CSR9_SROM_CS;
		bit_bang_delay();

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
	static const unsigned chip_dev[] = {
		TULIP_DEV_ID_21041, TULIP_DEV_ID_21143,
	};
	static const char *chip_name[] = {
		"21041", "21143",
	};
	unsigned id;

	id = pcicfg_read_word(dev, fnc, 0x00);
	if((id & 0xffff) != TULIP_VND_ID)
		return 0;
	id >>= 16;

	for(chip_id = 0; id != chip_dev[chip_id]; ++chip_id)
		if(chip_id >= elements(chip_dev))
			return 0;

	DPRINTF("tulip: #%d device %s\n", (dev != PCI_DEV_ETH0), chip_name[chip_id]);

	pcicfg_write_word(dev, fnc, 0x10, iob);

	pcicfg_write_half(dev, fnc, 0x04,
		pcicfg_read_half(dev, fnc, 0x04) | (1 << 0));

	pcicfg_write_byte(dev, fnc, 0x0c, DCACHE_LINE_SIZE / 4);

	pcicfg_write_byte(dev, fnc, 0x0d, 64);

	return 1;
}

/*
 * initialise network controller
 */
void tulip_init(void)
{
	nic_avail = tulip_setup(PCI_DEV_ETH0, PCI_FNC_ETH0, IO_BASE_ETH0);
	tulip_setup(PCI_DEV_ETH1, PCI_FNC_ETH1, IO_BASE_ETH1);

	if(!nic_avail)
		return;

	/* wake up device */

	pcicfg_write_word(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x40, 0x00000000);
	udelay(1000);

	/* reset device */

	reg_csr6 = CSR6_21143_MBO | CSR6_21143_HBD | CSR6_21143_PS | CSR6_FD;

	CSR(0) = CSR0_SWR;
	udelay(1000);

	CSR(0) = 0;
	udelay(1000);

	CSR(6) = reg_csr6;
	udelay(1000);

	read_hw_addr();
}

/*
 * initialise SIA PHY
 */
static void setup_phy_sia(void)
{
	unsigned mark, info;

	assert(chip_id == CHIP_ID_21041);

	if(phy_state == PHY_UNINIT) {

		CSR(13) = 0;
		CSR(14) = CSR14_SPP | CSR14_APE | CSR14_LTE | CSR14_SQE | CSR14_CLD |
			CSR14_CSQ | CSR14_RSQ | CSR14_ANE | CSR14_MBO | CSR14_CPEN_NORMAL |
			CSR14_LSE | CSR14_DREN | CSR14_LBK | CSR14_ECEN;
		CSR(15) = CSR15_ABM;
		CSR(13) = CSR13_SDM | CSR13_SRL;

		udelay(10 * 1000);
	}

	/* wait for link */

	phy_state = PHY_LINK_DOWN;

	for(mark = MFC0(CP0_COUNT);;) {

		info = CSR(12);
		if((info & (CSR12_ANS_MASK | CSR12_21041_LKF)) == CSR12_ANS_COMPLETE) {
			phy_state = PHY_LINK_UP;
			break;
		}

		if(MFC0(CP0_COUNT) - mark >= LINK_WAIT * CP0_COUNT_RATE)
			break;

		udelay(10 * 1000);
	}

#ifdef _DEBUG

	if(phy_state == PHY_LINK_UP) {

		if((info & (CSR12_LPC_10FDX | CSR12_LPN)) == (CSR12_LPC_10FDX | CSR12_LPN))
			DPUTS("tulip: link up (10Mbps full-duplex)");
		else
			DPUTS("tulip: link up (10Mbps)");
	}

#endif
}

/*
 * initialise MII PHY
 */
static void setup_phy_mii(void)
{
	unsigned mark, info;

	assert(chip_id == CHIP_ID_21143);

	if(phy_state == PHY_UNINIT) {

		CSR(13) = 0;
		CSR(14) = 0;
		CSR(15) = CSR15_ABM;

		udelay(10 * 1000);

		phy_write_mii(PHY_ID, PHY_REG_CTRL, PHY_REG_CTRL_RESET);

		udelay(10 * 1000);
	}

	/* wait for link */

	phy_state = PHY_LINK_DOWN;

	for(mark = MFC0(CP0_COUNT);;) {

		info = phy_read_mii(PHY_ID, PHY_REG_CSTAT);
		if(info & PHY_REG_CSTAT_LINK) {
			phy_state = PHY_LINK_UP;
			break;
		}

		if(MFC0(CP0_COUNT) - mark >= LINK_WAIT * CP0_COUNT_RATE)
			break;

		udelay(10 * 1000);
	}

#ifdef _DEBUG

	if(phy_state == PHY_LINK_UP) {

		static const char *link[] = {
			"10Mbps", "100Mbps", "10Mbps full-duplex", "100Mbps full-duplex",
		};

		DPRINTF("tulip: link up (%s)\n", link[(info >> 11) & 3]);
	}

#endif
}

/*
 * start transmitter and receiver
 */
int tulip_up(void)
{
	assert(!net_is_up());

	if(!nic_avail)
		return 0;

	if(chip_id == CHIP_ID_21041)
		setup_phy_sia();
	else
		setup_phy_mii();

	if(phy_state != PHY_LINK_UP) {
		DPUTS("tulip: link down");
		return 0;
	}

	/* enable/disable full duplex */

	reg_csr6 |= CSR6_FD;
	if(chip_id == CHIP_ID_21143 && !(phy_read_mii(PHY_ID, PHY_REG_CSTAT) & PHY_REG_CSTAT_DUPLEX))
		reg_csr6 &= ~CSR6_FD;

	/* reset rings */

	rx_ring_init();
	tx_ring_init();

	/* enable BM DMA */

	pcicfg_write_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04,
		pcicfg_read_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04) | (1 << 2));
	udelay(1000);

	/* start transmitter and receiver */

	CSR(6) = reg_csr6;
	CSR(6) = reg_csr6 | CSR6_ST | CSR6_SR;

	rx_filter_init();

	return 1;
}

/*
 * stop transmitter and receiver
 */
void tulip_down(void)
{
	assert(net_is_up());

	/* stop transmitter and receiver */

	CSR(6) = reg_csr6;
	while((CSR(5) & (CSR5_TS_MASK | CSR5_RS_MASK)) != (CSR5_TS_STOPPED | CSR5_RS_STOPPED))
		udelay(1000);

	/* disable BM DMA */

	pcicfg_write_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04,
		pcicfg_read_half(PCI_DEV_ETH0, PCI_FNC_ETH0, 0x04) & ~(1 << 2));
	udelay(1000);
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
