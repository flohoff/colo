/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "galileo.h"
#include "cobalt.h"

#define UART_REGISTER				((volatile uint8_t *) BRDG_NCS1_BASE)

#define UART_THR						(UART_REGISTER[0])
#define UART_RHR						(UART_REGISTER[0])
#define UART_FCR						(UART_REGISTER[2])
# define UART_FCR_FIFO_EN			(1 << 0)
#define UART_LCR						(UART_REGISTER[3])
# define UART_LCR_DATA8				(3 << 0)
# define UART_LCR_STOP2				(1 << 2)
# define UART_LCR_DL_EN				(1 << 7)
#define UART_MCR						(UART_REGISTER[4])
# define UART_MCR_DTR				(1 << 0)
# define UART_MCR_RTS				(1 << 1)
# define UART_MCR_OP1				(1 << 2)
#define UART_LSR						(UART_REGISTER[5])
# define UART_LSR_RDR				(1 << 0)
# define UART_LSR_THRE				(1 << 5)
# define UART_LSR_TEMPTY			(1 << 6)

#define UART_BRL						(UART_REGISTER[0])
#define UART_BRH						(UART_REGISTER[1])

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
static char out_queue[1024];
static unsigned baud;
static enum { ST_UNINIT = 0, ST_DISABLED, ST_ENABLED } state;

static unsigned stored_baud(void)
{
	if(!nv_store.baud || nv_store.baud > elements(rates))
		return BAUD_RATE;

	return rates[nv_store.baud - 1];
}

static void flush_ring(void)
{
	if(state == ST_ENABLED)

		while(queue_in != queue_out) {

			while(!(UART_LSR & UART_LSR_THRE))
				;

			UART_THR = out_queue[queue_out++ % sizeof(out_queue)];
		}
}

void serial_enable(int enable)
{
	static char buf[16];
	unsigned div;

	if(!enable) {

		if(state == ST_ENABLED) {
			state = ST_DISABLED;
			env_remove_tag(VAR_SERIAL);
		}

		return;
	}
	
	if(state == ST_UNINIT) {

		baud = stored_baud();
		div = (18432000 + baud * 8) / (baud * 16);

		UART_MCR = UART_MCR_OP1 | UART_MCR_RTS | UART_MCR_DTR;
		UART_LCR = UART_LCR_DL_EN | UART_LCR_STOP2 | UART_LCR_DATA8;
		UART_BRL = div;
		UART_BRH = div >> 8;
		UART_LCR = UART_LCR_STOP2 | UART_LCR_DATA8;
		UART_FCR = UART_FCR_FIFO_EN;
	}

	state = ST_ENABLED;

	flush_ring();

	sprintf(buf, "%u", baud);
	env_put("console-speed", buf, VAR_SERIAL);
}

void drain(void)
{
	if(state == ST_ENABLED)
		while(~UART_LSR & (UART_LSR_THRE | UART_LSR_TEMPTY))
			yield();
}

int kbhit(void)
{
	if(state == ST_ENABLED && (UART_LSR & UART_LSR_RDR))
		return 1;

	yield();

	return 0;
}

int getch(void)
{
	while(!kbhit())
		;

	return UART_RHR;
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
