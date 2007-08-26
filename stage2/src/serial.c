/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 *
 * NOTE
 *
 * There is support for some PCI serial I/O cards, however the cards
 * will probably not work without hardware modification. The Qube2 does
 * not supply -12v to the PCI bus, and the Qube doesn't supply +12v
 * either :-(
 */

#include "lib.h"
#include "galileo.h"
#include "cobalt.h"
#include "pci.h"
#include "cpu.h"

#define TIMEDIA_VND_ID				0x1409
#define TIMEDIA_DEV_ID				0x7168
#define INTASHIELD_VND_ID			0x135a
#define INTASHIELD_DEV_ID			0x0d80

#define PCI_BASE_ADDR				0x10108000

#define _UART_THR(p)					((p)->base[0])
#define _UART_RHR(p)					((p)->base[0])
#define _UART_BRL(p)					((p)->base[0])
#define _UART_BRH(p)					((p)->base[1])
#define _UART_FCR(p)					((p)->base[2])
#define _UART_LCR(p)					((p)->base[3])
#define _UART_MCR(p)					((p)->base[4])
#define _UART_LSR(p)					((p)->base[5])
#define _UART_SCR(p)					((p)->base[7])

#define UART_THR						_UART_THR(uart_port)
#define UART_RHR						_UART_RHR(uart_port)
#define UART_BRL						_UART_BRL(uart_port)
#define UART_BRH						_UART_BRH(uart_port)
#define UART_FCR						_UART_FCR(uart_port)
# define UART_FCR_FIFO_EN			(1 << 0)
#define UART_LCR						_UART_LCR(uart_port)
# define UART_LCR_DATA8				(3 << 0)
# define UART_LCR_STOP2				(1 << 2)
# define UART_LCR_DL_EN				(1 << 7)
#define UART_MCR						_UART_MCR(uart_port)
# define UART_MCR_DTR				(1 << 0)
# define UART_MCR_RTS				(1 << 1)
# define UART_MCR_OP1				(1 << 2)
#define UART_LSR						_UART_LSR(uart_port)
# define UART_LSR_RDR				(1 << 0)
# define UART_LSR_THRE				(1 << 5)
# define UART_LSR_TEMPTY			(1 << 6)
#define UART_SCR						_UART_SCR(uart_port)

struct uart_info
{
	char const			*name;
	uint8_t volatile	*base;
	unsigned				clock;
};

static const unsigned rates[] =
{
	50,
	75,
	110,
	150,
	300,
	600,
	1200,
	2400,
	4800,
	9600,
	14400,
	19200,
	28800,
	38400,
	57600,
	76800,
	115200,
	153600,
	230400,
};

static unsigned queue_in, queue_out;
static char out_queue[2048];
static unsigned baud;
static enum { ST_UNINIT = 0, ST_DISABLED, ST_ENABLED } state;
static struct uart_info uart_ports[4];
static struct uart_info const *uart_port;
static unsigned num_ports;

static int port_verify(struct uart_info const *port)
{
	unsigned token;

	token = 0;

	do {

		_UART_SCR(port) = token;
		_UART_LSR(port);
		_UART_MCR(port) = UART_MCR_OP1 | UART_MCR_RTS | UART_MCR_DTR;
		if (_UART_SCR(port) != token)
			return 0;

		token = (token + 111) & 0xff;

	} while(token);

	return 1;
}

static void port_add(struct uart_info const *info)
{
	if (num_ports < elements(uart_ports) && port_verify(info)) {
		printf("serial: port %u, %s\n", num_ports, info->name);
		uart_ports[num_ports++] = *info;
	}
}

static void scan_onboard(void)
{
	static struct uart_info const onboard =
	{
		.name		= "on-board",
		.base		= (volatile uint8_t *) BRDG_NCS1_BASE,
		.clock	= 18432000,
	};

	port_add(&onboard);
}

static void slot_enable(unsigned bar)
{
	pcicfg_write_word(PCI_DEV_SLOT, PCI_FNC_SLOT, bar, PCI_BASE_ADDR);

	pcicfg_write_half(PCI_DEV_SLOT, PCI_FNC_SLOT, 0x04,
		pcicfg_read_half(PCI_DEV_SLOT, PCI_FNC_SLOT, 0x04) | (1 << 0));
}

static void scan_pci_slot(void)
{
	static struct uart_info const intashield[2] =
	{
		{
			.name		= "Intashield #1",
			.base		= (volatile uint8_t *) KSEG1(PCI_BASE_ADDR),
			.clock	= 1843200,
		},
		{
			.name		= "Intashield #2",
			.base		= (volatile uint8_t *) KSEG1(PCI_BASE_ADDR) + 8,
			.clock	= 1843200,
		},
	};
	static struct uart_info const timedia =
	{
		.name		= "Timedia",
		.base		= (volatile uint8_t *) KSEG1(PCI_BASE_ADDR),
		.clock	= 14745600,
	};

	switch(pcicfg_read_word(PCI_DEV_SLOT, PCI_FNC_SLOT, 0x00)) {

		case ((TIMEDIA_DEV_ID << 16) | TIMEDIA_VND_ID):
			slot_enable(0x10);
			port_add(&timedia);
			break;

		case ((INTASHIELD_DEV_ID << 16) | INTASHIELD_VND_ID):
			slot_enable(0x18);
			port_add(&intashield[0]);
			port_add(&intashield[1]);
			break;

		default:
			return;
	}
}

static unsigned stored_baud(void)
{
	if(!nv_store.baud || nv_store.baud > elements(rates))
		return BAUD_RATE;

	return rates[nv_store.baud - 1];
}

static unsigned flush_netcon(void)
{
	unsigned indx, copy, fill;

	for(copy = queue_out;;) {

		fill = queue_in - copy;
		if(!fill)
			break;

		indx = copy % sizeof(out_queue);
		if(indx + fill > sizeof(out_queue))
			fill = sizeof(out_queue) - indx;

		fill = netcon_write(out_queue + indx, fill);
		if(!fill)
			break;

		copy += fill;
	}

	return copy - queue_out;
}

static void flush_ring(void)
{
	unsigned copy;

	copy = flush_netcon();

	if(state == ST_ENABLED)

		while(queue_in != queue_out) {

			while(!(UART_LSR & UART_LSR_THRE))
				;

			UART_THR = out_queue[queue_out++ % sizeof(out_queue)];
		}

	else

		queue_out += copy;
}

void serial_scan(void)
{
	num_ports = 0;

	if(nv_store.flags & NVFLAG_CONSOLE_PCI_SERIAL) {
		scan_pci_slot();
		scan_onboard();
	} else {
		scan_onboard();
		scan_pci_slot();
	}
}

void serial_enable(int enable)
{
	static char buf[16];
	unsigned div;

	if(!enable) {

		if(state == ST_ENABLED) {
			state = ST_DISABLED;
			env_put("console-speed", NULL, 0);
		}

		return;
	}
	
	if(state == ST_UNINIT) {

		if (!num_ports)
			return;

		uart_port = &uart_ports[0];

		baud = stored_baud();
		div = (uart_port->clock + baud * 8) / (baud * 16);

		UART_MCR = UART_MCR_OP1 | UART_MCR_RTS | UART_MCR_DTR;
		UART_LCR = UART_LCR_DL_EN | UART_LCR_STOP2 | UART_LCR_DATA8;
		UART_BRL = div;
		UART_BRH = div >> 8;
		UART_LCR = UART_LCR_STOP2 | UART_LCR_DATA8;
		UART_FCR = UART_FCR_FIFO_EN;
	}

	state = ST_ENABLED;

	sprintf(buf, "%u", baud);
	env_put("console-speed", buf, VAR_OTHER);

	flush_ring();
}

void drain(void)
{
	if(state == ST_ENABLED)
		while(~UART_LSR & (UART_LSR_THRE | UART_LSR_TEMPTY))
			yield();
}

/*
 * don't hog the PCI bus whilst polling the serial
 */
int kbhit(void)
{
	static unsigned hit, mark;
	unsigned diff;

	if(state == ST_ENABLED) {

		if(hit && (UART_LSR & UART_LSR_RDR))
				return 1;

		diff = MFC0(CP0_COUNT) - mark;

		if(diff >= CP0_COUNT_RATE / 100) {

			mark += diff;

			hit = UART_LSR & UART_LSR_RDR;
			if(hit)
				return 1;
		}
	}

	if(netcon_poll())
		return 1;

	yield();

	return 0;
}

int getch(void)
{
	uint8_t tmp;

	while(!kbhit())
		;

	if(state == ST_ENABLED && (UART_LSR & UART_LSR_RDR))
		return UART_RHR;

	if(netcon_read(&tmp, 1))
		return tmp;

	return 0;
}

void putchar(int chr)
{
	if(chr == '\n')
		putchar('\r');

	if(queue_in - queue_out >= sizeof(out_queue))
		++queue_out;

	out_queue[queue_in++ % sizeof(out_queue)] = chr;

	flush_ring();
}

void putstring(const char *str)
{
	while(*str)
		putchar(*str++);
}

void puts(const char *str)
{
	putstring(str);
	putchar('\n');
}

int cmnd_serial(int opsz)
{
	unsigned size, indx, error, best;
	unsigned long val;
	char *end;

	if(argc < 2) {
		printf("%u\n", stored_baud());
		return E_NONE;
	}

	if(argc > 2)
		return E_ARGS_OVER;

	size = strlen(argv[1]);

	if(!strncasecmp(argv[1], "default", size)) {

		puts(_STR(BAUD_RATE));

		nv_store.baud = 0;
		nv_put();

		return E_NONE;

	} else if(!strncasecmp(argv[1], "on", size)) {

		serial_enable(1);

		return E_NONE;

	} else if(!strncasecmp(argv[1], "off", size)) {

		serial_enable(0);

		return E_NONE;
	}

	val = strtoul(argv[1], &end, 10);
	if(*end || end == argv[1])
		return E_BAD_EXPR;
	if(!val)
		return E_BAD_VALUE;

	best = 0;

	for(indx = 0; indx < elements(rates); ++indx) {

		if(val >= rates[indx])
			error = val - rates[indx];
		else
			error = rates[indx] - val;

		error = (error * 1000 + val / 2) / val;

		if(indx && error > best)
			break;

		best = error;
	}

	printf("%u\n", rates[indx - 1]);

	nv_store.baud = indx;
	nv_put();

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
