/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "panel.h"

#define APP_NAME						"e2fsck-lcd"
#define EXTENSION						".distrib"
#define CHILD_DEFAULT				"fsck.ext2"
#define ERROR_EXIT					8						/* e2fsck - operational error */

#define LCD_WIDTH						16

static const char spaces[] = { [0 ... (LCD_WIDTH - 1)] = ' ' };
static unsigned thumb;
static int lcd;

const char *getapp(void)
{
	return APP_NAME;
}

/*
 * initialise output
 */
static void disp_init(const char *devn)
{
	lcd = lcd_open();
	if(!lcd)
		return;

	lcd_clear();

	if(devn) {

		lcd_curs_move(0, 0);
		lcd_text(devn, LCD_WIDTH - 7);
	}

	lcd_curs_move(0, LCD_WIDTH - 4);
	lcd_text("0.0%", -1);

	lcd_curs_move(1, 0);
	lcd_text("[>", -1);
	lcd_curs_move(1, LCD_WIDTH - 1);
	lcd_text("]", -1);

	thumb = 0;
}

/*
 * finish output
 */
static void disp_cleanup(void)
{
	usleep(500 * 1000);

	if(lcd)
		lcd_clear();

	fputs("       \r", stdout);
	fflush(stdout);
}

/*
 * update output with new progress information
 */
static void disp_update(int pass, unsigned where, unsigned limit)
{
	static char bar[] = { [0 ... (LCD_WIDTH - 1)] = '=' };
	static char text[16];
	unsigned prog;

	if(pass < 1 || pass > 5)
		return;

	prog = (pass - 1) * 200 + (where * 200 + limit / 2) / limit;

	snprintf(text, sizeof(text), "%3u.%u%%", prog / 10, prog % 10);
	text[sizeof(text) - 1] = '\0';

	if(lcd) {

		lcd_curs_move(0, LCD_WIDTH - 6);
		lcd_text(text, 6);

		prog = ((LCD_WIDTH - 2) * prog + 500) / 1000;
		if(prog > thumb) {
			lcd_curs_move(1, thumb + 1);
			lcd_text(bar, prog - thumb);
			thumb = prog;
			if(thumb < LCD_WIDTH - 2)
				lcd_text(">", -1);
		}
	}

	printf(" %s\r", text);
	fflush(stdout);
}

/*
 * read progress information from pipe
 */
static void monitor(int fd, const char *devn)
{
	static char ibuf[8192];

	unsigned fill, pass, where, limit;
	int cntl, nread, skip, init;
	char pad, *ptr;
	fd_set set;

	cntl = fcntl(fd, F_GETFL);
	if(cntl == -1 || fcntl(fd, F_SETFL, cntl | O_NONBLOCK)) {
		fprintf(stderr, APP_NAME ": failed to set pipe to non-blocking (%s)\n", strerror(errno));
		return;
	}

	if(devn) {
		ptr = strrchr(devn, '/');
		if(ptr)
			devn = ptr + 1;
	}

	FD_ZERO(&set);

	for(init = 0, fill = 0, skip = 0;;) {

		FD_SET(fd, &set);

		if(select(fd + 1, &set, NULL, NULL, NULL) == -1 && errno != EINTR) {
			fprintf(stderr, APP_NAME ": failed waiting on pipe (%s)\n", strerror(errno));
			return;
		}

		nread = read(fd, ibuf + fill, sizeof(ibuf) - fill);
		if(!nread)
			break;

		if(nread == -1) {
			if(errno != EINTR && errno != EAGAIN) {
				fprintf(stderr, APP_NAME ": failed reading from pipe (%s)\n", strerror(errno));
				return;
			}
			continue;
		}

		fill += nread;

		for(;;) {

			ptr = strchr(ibuf, '\n');
			if(!ptr) {

				if(fill == sizeof(ibuf)) {

					if(!skip)
						fputs(APP_NAME ": line too long\n", stderr);

					skip = 1;
					fill = 0;
				}

				break;
			}

			*ptr++ = '\0';

			if(!skip) {

				if(sscanf(ibuf, " %u %u %u %c", &pass, &where, &limit, &pad) == 3) {

					if(!init) {
						disp_init(devn);
						init = 1;
					}

					disp_update(pass, where, limit);

				} else

					fputs(APP_NAME ": line invalid format\n", stderr);
			}

			skip = 0;

			fill -= ptr - ibuf;
			memmove(ibuf, ptr, fill);
		}
	}

	if(init > 0)
		disp_cleanup();
}

int main(int argc, char *argv[])
{
	static char path[PATH_MAX] = CHILD_DEFAULT;

	char *ptr, *end, *desc, *devn, *argp[argc + 1];
	int next, retn, tube[2];
	pid_t child, dead;
	struct stat info;
	unsigned indx;

	/* work out executable to spawn */

	ptr = strrchr(argv[0], '/') + 1;
	if(ptr == (char *) NULL + 1)
		ptr = argv[0];

	if(strcmp(ptr, APP_NAME)) {
		ptr = path + strlen(argv[0]);
		if(ptr + sizeof(EXTENSION) > path + sizeof(path)) {
			fputs(APP_NAME ": executable path too long\n", stderr);
			return ERROR_EXIT;
		}
		strcpy(path, argv[0]);
		strcpy(ptr, EXTENSION);
	}

	argp[0] = path;
	for(indx = 1; indx < argc; ++indx)
		argp[indx] = argv[indx];
	argp[indx] = NULL;

	/* find argument of -C option and device node */

	desc = NULL;
	devn = NULL;
	next = 0;

	for(indx = 0;; ++indx) {

		ptr = argp[indx];
		if(!ptr)
			break;

		if(next) {
			next = 0;
		} else if(*ptr == '-') {
			ptr = strchr(ptr, 'C');
			if(!ptr)
				continue;
			next = !*++ptr;
			if(next)
				continue;
		} else if(strchr(ptr, '/') && !stat(ptr, &info) && S_ISBLK(info.st_mode)) {
			devn = ptr;
			continue;
		}

		if(!desc) {
			strtoul(ptr, &end, 0);
			if(end > ptr && !*end)
				desc = ptr;
		}
	}

	if(desc) {

		/* start e2fsck as child */

		close(3);
		close(4);

		if(pipe(tube))

			fprintf(stderr, APP_NAME ": failed to create pipe (%s)\n", strerror(errno));

		else if(tube[1] < 10) {

			child = fork();
			switch(child) {

				case -1:
					fprintf(stderr, APP_NAME ": failed to fork child (%s)\n", strerror(errno));
					break;

				default:

					close(tube[1]);

					monitor(tube[0], devn);

					close(tube[0]);		/* force SIGPIPE, what else can we do ? */

					for(;;) {

						dead = wait(&retn);
						if(dead == child)
							break;

						if(dead == -1 && errno != EINTR) {
							fprintf(stderr, APP_NAME ": failed waiting for child to exit (%s)\n", strerror(errno));
							return ERROR_EXIT;
						}
					}

					if(WIFSIGNALED(retn)) {
						fprintf(stderr, APP_NAME ": child exited on signal #%d\n", WTERMSIG(retn));
						return ERROR_EXIT;
					}

					return WEXITSTATUS(retn);

				case 0:

					close(tube[0]);

					/* replace -C argument with our descriptor */

					desc[0] = '0' + tube[1];
					desc[1] = '\0';
			}
		}
	}

	execvp(argp[0], argp);

	fprintf(stderr, APP_NAME ": failed to exec \"%s\" (%s)\n", argp[0], strerror(errno));

	return ERROR_EXIT;
}

/* vi:set ts=3 sw=3 cin: */
