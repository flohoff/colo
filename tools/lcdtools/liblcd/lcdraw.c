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

#include "liblcd.h"

#define BTN_PHYS_ADDR			0x1d000000
#define BTN_PHYS_SIZE			4

#define BTN_READ()				(~btn[0]>>24)

#define LCD_PHYS_ADDR			0x1f000000
#define LCD_PHYS_SIZE			0x20

#define LCD_READ(r)				(lcd[!!(r)*4]>>24)
#define LCD_WRITE(r,v)			do{lcd[!!(r)*4]=(unsigned)(v)<<24;}while(0)

#define LCD_BUSY					(1 << 7)
#define LCD_CLEAR					0x01
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

static void exp_clear(void)
{
	lcd_write(0, LCD_CLEAR);
}

static void exp_prog(unsigned which, const void *data)
{
	unsigned indx;

	lcd_write(0, LCD_CGRAM_ADDR | (which * 8));

	for(indx = 0; indx < 8; ++indx)
		lcd_write(1, ((uint8_t *) data)[indx]);

	lcd_write(0, LCD_DDRAM_ADDR);
}

static void exp_curs_move(unsigned row, unsigned col)
{
	lcd_write(0, LCD_DDRAM_ADDR | (LCD_ROW_OFFSET * row + col));
}

static void exp_text(const char *str, unsigned max)
{
	unsigned indx;

	for(indx = 0; str[indx] && indx < max; ++indx)
		lcd_write(1, str[indx]);
}

static void exp_puts(unsigned row, unsigned col, unsigned wid, const char *str)
{
	unsigned indx;

	exp_curs_move(row, col);

	for(indx = 0; str[indx] && indx < wid; ++indx)
		lcd_write(1, str[indx]);

	for(; indx < wid; ++indx)
		lcd_write(1, ' ');
}

static int exp_buttons(void)
{
	return BTN_READ() & BTN_MASK;
}

static void exp_close(void)
{
	munmap((void *) lcd, LCD_PHYS_SIZE);
	munmap((void *) btn, BTN_PHYS_SIZE);
}

static const struct lcd_dispatch_table funcs =
{
	.close		= exp_close,
	.clear		= exp_clear,
	.prog_char	= exp_prog,
	.puts			= exp_puts,
	.text			= exp_text,
	.curs_move	= exp_curs_move,
	.buttons		= exp_buttons,
};

const struct lcd_dispatch_table *lcdraw_open(void)
{
	int fd;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd == -1) {
		fprintf(stderr, "%s: failed to open /dev/mem (%s)\n", getapp(), strerror(errno));
		return NULL;
	}

	lcd = mmap(NULL, LCD_PHYS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, LCD_PHYS_ADDR);
	if(lcd == MAP_FAILED) {
		fprintf(stderr, "%s: failed to map /dev/mem (%s)\n", getapp(), strerror(errno));
		close(fd);
		return NULL;
	}

	btn = mmap(NULL, BTN_PHYS_SIZE, PROT_READ, MAP_SHARED, fd, BTN_PHYS_ADDR);
	if(btn == MAP_FAILED) {
		fprintf(stderr, "%s: failed to map /dev/mem (%s)\n", getapp(), strerror(errno));
		munmap((void *) lcd, LCD_PHYS_SIZE);
		close(fd);
		return NULL;
	}

	close(fd);

	return &funcs;
}

/* vi:set ts=3 sw=3 cin: */
