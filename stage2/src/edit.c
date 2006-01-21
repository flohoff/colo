/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "keymap.h"

static unsigned curr;
static unsigned next;
static size_t buffsz;
static char *buff;

static void update_eol(int erase)
{
	unsigned indx;

	for(indx = next; indx < buffsz; ++indx)
		putchar(buff[indx]);

	for(indx = 0; indx < erase; ++indx)
		putchar(' ');

	erase += buffsz;
	for(indx = next; indx < erase; ++indx)
		putchar('\b');
}

static void move_left(unsigned count)
{
	while(curr && count--) {
		putchar('\b');
		buff[--next] = buff[--curr];
	}
}

static void move_right(unsigned count)
{
	while(next < buffsz && count--) {
		putchar(buff[next]);
		buff[curr++] = buff[next++];
	}
}

static void replace(const char *text, int where)
{
	unsigned erase, indx;
	size_t len;

	len = text ? strlen(text) : 0;
	if((unsigned) where > len)
		where = len;

	erase = curr + buffsz - next;
	if(erase <= len)
		erase = 0;
	else
		erase -= len;

	len -= where;

	for(; curr; --curr)
		putchar('\b');

	for(; curr < where; ++curr) {
		buff[curr] = *text++;
		putchar(buff[curr]);
	}

	next = buffsz - len;
	for(indx = 0; indx < len; ++indx) {
		putchar(text[indx]);
		buff[next + indx] = text[indx];
	}

	for(indx = erase; indx--;)
		putchar(' ');

	for(indx = erase + len; indx--;)
		putchar('\b');
}

static int history(int chr)
{
	unsigned save, count, which;
	char text[buffsz];

	count = history_count();
	if(!count)
		return kgetch();

	memmove(&buff[curr], &buff[next], buffsz - next);
	buff[curr + buffsz - next] = '\0';
	history_add(buff);
	++count;

	save = curr;

	for(which = 0;; chr = kgetch()) {

		switch(chr) {

			case KEY_HISTORY_MATCH:
				do {

					if(++which == count)
						which = 0;

					history_fetch(text, sizeof(text), which);

				} while(which && strncmp(buff, text, save));

				replace(text, which ? -1 : save);
				continue;

			case KEY_HISTORY_NEXT:
				if(!which)
					which = count;
				--which;
				break;

			case KEY_HISTORY_PREV:
				if(++which == count)
					which = 0;
				break;

			case KEY_CLEAR:
				if(which) {
					which = 0;
					break;
				}

				/* */

			default:
				history_discard();
				return chr;
		}

		history_fetch(text, sizeof(text), which);
		replace(text, which ? -1 : save);
	}
}

static unsigned word_left(void)
{
	unsigned mark;

	mark = curr;

	while(mark && isspace(buff[mark - 1]))
		--mark;
	while(mark && !isspace(buff[mark - 1]))
		--mark;

	return curr - mark;
}

static unsigned word_right(void)
{
	unsigned mark;

	mark = next;
	
	while(mark < buffsz && !isspace(buff[mark + 1]))
		++mark;
	while(mark < buffsz && isspace(buff[mark + 1]))
		++mark;

	return mark - next;
}

void line_edit(char *line, size_t size)
{
	unsigned count;
	int chr;

	buff = line;
	buffsz = size;

	curr = 0;
	next = buffsz;

	for(chr = kgetch();;) {

		switch(chr) {

			case KEY_HOME:
				move_left(buffsz);
				break;

			case KEY_END:
				move_right(buffsz);
				break;

			case KEY_CURSOR_LEFT:
				move_left(1);
				break;

			case KEY_CURSOR_RIGHT:
				move_right(1);
				break;

			case KEY_CLEAR:
				replace(NULL, 0);
				break;

			case KEY_DELETE:
				if(next < buffsz) {
					++next;
					update_eol(1);
				}
				break;

			case KEY_BACKSPACE:
				if(curr) {
					putchar('\b');
					update_eol(1);
					--curr;
				}
				break;

			case KEY_HISTORY_PREV:
			case KEY_HISTORY_NEXT:
			case KEY_HISTORY_MATCH:
				chr = history(chr);
				continue;

			case KEY_ENTER:
				move_right(buffsz - next);
				buff[curr] = '\0';
				return;

			case KEY_WORD_RIGHT:
				move_right(word_right());
				break;

			case KEY_WORD_LEFT:
				move_left(word_left());
				break;

			case KEY_DELETE_WORD:
				count = word_left();
				move_left(count);
				next += count;
				update_eol(count);
				break;

			default:
				if(isprint(chr) && curr < next) {
					putchar(chr);
					update_eol(0);
					buff[curr++] = chr;
				}
		}

		chr = kgetch();
	}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
