/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include "panel.h"

#define BTN_PHYS_ADDR			0x1d000000
#define BTN_PHYS_SIZE			4

#define BTN_READ()				(~btn[0]>>24)

#define LCD_PHYS_ADDR			0x1f000000
#define LCD_PHYS_SIZE			0x20

#define LCD_READ(r)				(lcd[!!(r)*4]>>24)
#define LCD_WRITE(r,v)			do{lcd[!!(r)*4]=(unsigned)(v)<<24;}while(0)

#define LCD_BUSY					(1 << 7)
#define LCD_CLEAR					0x0c
#define LCD_CGRAM_ADDR			0x40
#define LCD_DDRAM_ADDR			0x80
#define LCD_ROW_OFFSET			0x40

static volatile uint32_t *btn;
static volatile uint32_t *lcd;

static void lcd_write(int reg, unsigned val)
{
	while(LCD_READ(0) & LCD_BUSY)
		usleep(1);
	
	usleep(10);

	LCD_WRITE(reg, val);
}

void lcd_clear(void)
{
	lcd_write(0, LCD_CLEAR);
}

void lcd_prog(unsigned which, const void *data)
{
	unsigned indx;

	lcd_write(0, LCD_CGRAM_ADDR | (which * 8));

	for(indx = 0; indx < 8; ++indx)
		lcd_write(1, ((uint8_t *) data)[indx]);

	lcd_write(0, LCD_DDRAM_ADDR);
}

void lcd_curs_move(unsigned row, unsigned col)
{
	lcd_write(0, LCD_DDRAM_ADDR | (LCD_ROW_OFFSET * row + col));
}

void lcd_text(const char *str, unsigned max)
{
	unsigned indx;

	for(indx = 0; str[indx] && indx < max; ++indx)
		lcd_write(1, str[indx]);
}

void lcd_puts(unsigned row, unsigned col, unsigned wid, const char *str)
{
	unsigned indx;

	lcd_curs_move(row, col);

	for(indx = 0; str[indx] && indx < wid; ++indx)
		lcd_write(1, str[indx]);

	for(; indx < wid; ++indx)
		lcd_write(1, ' ');
}

int lcd_open(void)
{
	int fd;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd == -1) {
		fprintf(stderr, "%s: failed to open /dev/mem (%s)\n", getapp(), strerror(errno));
		return 0;
	}

	lcd = mmap(NULL, LCD_PHYS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, LCD_PHYS_ADDR);
	if(lcd == MAP_FAILED) {
		fprintf(stderr, "%s: failed to map /dev/mem (%s)\n", getapp(), strerror(errno));
		close(fd);
		return 0;
	}

	btn = mmap(NULL, BTN_PHYS_SIZE, PROT_READ, MAP_SHARED, fd, BTN_PHYS_ADDR);
	if(btn == MAP_FAILED) {
		fprintf(stderr, "%s: failed to map /dev/mem (%s)\n", getapp(), strerror(errno));
		munmap((void *) lcd, LCD_PHYS_SIZE);
		close(fd);
		return 0;
	}

	close(fd);

	return 1;
}

void lcd_close(void)
{
	munmap((void *) lcd, LCD_PHYS_SIZE);
	munmap((void *) btn, BTN_PHYS_SIZE);
}

int btn_read(void)
{
	return BTN_READ() & BTN_MASK;
}

/* vi:set ts=3 sw=3 cin: */
