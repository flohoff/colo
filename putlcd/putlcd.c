/*
 * (C) P.Horton 2004
 *
 * $Id$
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#define APP_NAME					"putlcd"

#define LCD_WIDTH					16

#define LCD_PHYS_ADDR			0x1f000000

#define LCD_READ(r)				(lcd[!!(r)*4]>>24)
#define LCD_WRITE(r,v)			do{lcd[!!(r)*4]=(unsigned)(v)<<24;}while(0)

#define LCD_BUSY					(1 << 7)
#define LCD_DDRAM_ADDR			0x80
#define LCD_ROW_OFFSET			0x28

static volatile uint32_t *lcd;

static void lcd_write(int reg, unsigned val)
{
	while(LCD_READ(0) & LCD_BUSY)
		;
	
	usleep(10);

	LCD_WRITE(reg, val);
}

static void lcd_puts(unsigned indx, const char *str)
{
	lcd_write(0, LCD_DDRAM_ADDR | (LCD_ROW_OFFSET * !!indx));

	for(indx = 0; str[indx] && indx < LCD_WIDTH; ++indx)
		lcd_write(1, str[indx]);

	for(; indx < LCD_WIDTH; ++indx)
		lcd_write(1, ' ');
}

int main(int argc, char *argv[])
{
	int fd;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd == -1) {
		fprintf(stderr, APP_NAME ": failed to open /dev/mem (%s)\n", strerror(errno));
		return 1;
	}

	lcd = mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, LCD_PHYS_ADDR);
	if(lcd == MAP_FAILED) {
		fprintf(stderr, APP_NAME ": failed to map /dev/mem (%s)\n", strerror(errno));
		return 1;
	}

	lcd_puts(0, argc > 1 ? argv[1] : "");
	lcd_puts(1, argc > 2 ? argv[2] : "");

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
