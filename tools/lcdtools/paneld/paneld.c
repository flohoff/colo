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
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "liblcd.h"

#define APP_NAME					"paneld"

#define LCD_MENU_TIMEOUT		(-1)
#define LCD_MENU_CANCEL			(-2)

#define BTN_DEBOUNCE				(50 * 1000)
#define UPDATE_TICK				(500 * 1000)
#define UPDATE_PERIOD_COUNT	(5 * 1000 * 1000 / UPDATE_TICK)
#define MENU_SELECT_COUNT		(1000 * 1000 * 3 / 2 / UPDATE_TICK)
#define MENU_TIMEOUT				(10 * 1000 * 1000)
#define MENU_MESSAGE				(5 * 1000 * 1000)

#define ELEMENTS(x)				(sizeof(x)/sizeof((x)[0]))

static const char *menu_options[] =
{
	"ADMIN MENU",
	"Halt",
	"Reboot",
};

static const char *menu_messages[][2] =
{
	{ "Shutting down", "  for halt.", },
	{ "Shutting down", " for reboot.", },
};

static const char *action_halt[] =
{
	"/sbin/shutdown", "-a", "-t1", "-h", "now", NULL,
};

static const char *action_reboot[] =
{
	"/sbin/shutdown", "-a", "-t1", "-r", "now", NULL,
};

static const char **menu_actions[] =
{
	action_halt,
	action_reboot,
};

const char *getapp(void)
{
	return APP_NAME;
}

int lcd_menu(const char **options, unsigned count, unsigned timeout)
{
	static const uint8_t arrow[][8] =
	{
		{ 0x01, 0x03, 0x07, 0x0f, 0x07, 0x03, 0x01, 0x00, },
		{ 0x10, 0x18, 0x1c, 0x1e, 0x1c, 0x18, 0x10, 0x00, },
	};
	static char symbol[][2] =
	{
		" ", "]", "[",
	};
	unsigned exp, row, top, btn, prv;
	struct timeval mark, now;
	char buf[2];

	if(count < 2)
		return -1;

	if(lcd_prog(1, arrow[0]) && lcd_prog(2, arrow[1])) {
		symbol[1][0] = '\001';
		symbol[2][0] = '\002';
	}

	prv = btn_read();

	buf[1] = '\0';
	row = 1;
	top = 0;

	for(;;) {

		lcd_puts(0, 0, 1, row ? symbol[0] : symbol[2]);
		lcd_puts(0, 1, LCD_WIDTH - 2, options[top]);
		lcd_puts(0, LCD_WIDTH - 1, 1, row ? symbol[0] : symbol[1]);

		lcd_puts(1, 0, 1, row ? symbol[2] : symbol[0]);
		lcd_puts(1, 1, LCD_WIDTH - 2, options[top + 1]);
		lcd_puts(1, LCD_WIDTH - 1, 1, row ? symbol[1] : symbol[0]);

		gettimeofday(&mark, NULL);
		
		for(;;) {

			if(timeout) {

				gettimeofday(&now, NULL);

				exp = (now.tv_sec - mark.tv_sec) * 1000000 + now.tv_usec - mark.tv_usec;

				if(exp >= timeout)
					return LCD_MENU_TIMEOUT;
			}

			btn = btn_read();

			btn ^= prv;
			prv ^= btn;
			btn &= prv;

			if(btn & (BTN_RIGHT | BTN_ENTER | BTN_SELECT))
				return top + row - 1;

			if(btn & BTN_LEFT)
				return LCD_MENU_CANCEL;

			if(btn & BTN_UP) {
				if(top) {
					--top;
					row = 1;
					break;
				}
				if(row != 1) {
					row = 0;
					break;
				}
			}

			if(btn & BTN_DOWN) {
				if(top + 2 < count) {
					++top;
					row = 0;
					break;
				} 
				if(!row) {
					row = 1;
					break;
				}
			}

			usleep(BTN_DEBOUNCE);
		}
	}
}

static void usage(void)
{
	puts("usage: " APP_NAME " [ -d ]");
	exit(1);
}

int main(int argc, char *argv[])
{
	static char text[32];

	unsigned count, update, active, roller;
	int opt, daemon, which;
	time_t wall;

	daemon = 0;

	opterr = 0;

	while((opt = getopt(argc, argv, "d")) != -1)
		switch(opt) {
			case 'd':
				daemon = 1;
				break;
			default:
				usage();
		}

	if(!lcd_open(NULL))
		return -1;

	if(daemon)

		switch(fork()) {

			case -1:
				fprintf(stderr, APP_NAME ": failed to fork (%s)\n", strerror(errno));
				return -1;

			case 0:
				setsid();
				break;

			default:
				return 0;
		}

	for(roller = optind;;) {

		active = MENU_SELECT_COUNT;
		update = 0;

		for(count = UPDATE_PERIOD_COUNT;; ++count) {

			if(count - update >= UPDATE_PERIOD_COUNT) {

				update += UPDATE_PERIOD_COUNT;

				time(&wall);
				strftime(text, sizeof(text), "%a %b %d %H:%M", localtime(&wall));
				lcd_puts(0, 0, LCD_WIDTH, text);

				if(roller < argc) {

					lcd_puts(1, 0, LCD_WIDTH, argv[roller++]);

					if(roller == argc)
						roller = optind;

				} else

					lcd_puts(1, 0, LCD_WIDTH, "");
			}

			if(btn_read() & (BTN_SELECT | BTN_ENTER)) {
				if(active < MENU_SELECT_COUNT && ++active == MENU_SELECT_COUNT)
					break;
			} else
				active = 0;

			usleep(UPDATE_TICK);
		}

		which = lcd_menu(menu_options, ELEMENTS(menu_options), MENU_TIMEOUT);

		if(which >= 0 && which < ELEMENTS(menu_actions)) {

			lcd_puts(0, 0, LCD_WIDTH, menu_messages[which][0]);
			lcd_puts(1, 0, LCD_WIDTH, menu_messages[which][1]);

			switch(fork()) {

				case -1:
					fprintf(stderr, APP_NAME ": failed to fork (%s)\n", strerror(errno));
					break;

				case 0:
					lcd_close();

					execv(menu_actions[which][0], (char **) menu_actions[which]);

					fprintf(stderr, APP_NAME ": failed to exec \"%s\" (%s)\n", menu_actions[which][0], strerror(errno));
					return -1;
			}

			usleep(MENU_MESSAGE);
		}
	}

	return 0;
}

/* vi:set ts=3 sw=3 cin: */
