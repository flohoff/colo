/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage2/src/serial.c,v 1.2 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "galileo.h"

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

void serial_init(void)
{
	UART_MCR = UART_MCR_OP1 | UART_MCR_RTS | UART_MCR_DTR;
	UART_LCR = UART_LCR_DL_EN | UART_LCR_STOP2 | UART_LCR_DATA8;
	UART_BRL = (18432000 + 115200 * 8) / (115200 * 16);
	UART_BRH = 0x00;
	UART_LCR = UART_LCR_STOP2 | UART_LCR_DATA8;
	UART_FCR = UART_FCR_FIFO_EN;
}

void drain(void)
{
	while(~UART_LSR & (UART_LSR_THRE | UART_LSR_TEMPTY))
		;
}

int kbhit(void)
{
	return !!(UART_LSR & UART_LSR_RDR);
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

	while(!(UART_LSR & UART_LSR_THRE))
		;

	UART_THR = chr;
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

/* vi:set ts=3 sw=3 cin path=include,../include: */
