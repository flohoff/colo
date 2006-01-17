/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

static char hist_buf[8192];
static unsigned hist_indx[128];
static unsigned hist_head;
static unsigned hist_tail;

/*
 * add line to history buffer
 */
int history_add(const char *line)
{
	unsigned dest, used, top;
	size_t size;

	size = strlen(line);
	if(size > sizeof(hist_buf) / 2)
		return 0;

	/* discard oldest entry if hist_indx[] is full */

	if(hist_tail - hist_head == elements(hist_indx) - 1)
		++hist_head;

	/* discard old entries until we have space in hist_buf[] */

	for(dest = hist_indx[hist_tail % elements(hist_indx)];;)
	{
		used = dest - hist_indx[hist_head % elements(hist_indx)];
		if(used + size <= sizeof(hist_buf))
			break;

		++hist_head;

		assert(hist_tail != hist_head);
	}

	/* add item to hist_indx[] */

	hist_indx[++hist_tail % elements(hist_indx)] = dest + size;

	/* copy line to hist_buf[] */

	dest %= sizeof(hist_buf);
	top = sizeof(hist_buf) - dest;

	if(size > top) {
		memcpy(hist_buf, line + top, size - top);
		size = top;
	}
	memcpy(hist_buf + dest, line, size);

	return 1;
}

/*
 * fetch line from history buffer
 */
int history_fetch(char *line, size_t limit, unsigned which)
{
	unsigned ofs, size, top;

	if(which >= hist_tail - hist_head)
		return 0;

	which = hist_tail - (which + 1);

	ofs = hist_indx[which % elements(hist_indx)];
	size = hist_indx[++which % elements(hist_indx)] - ofs;

	if(size >= limit)
		size = limit - 1;

	ofs %= sizeof(hist_buf);
	top = sizeof(hist_buf) - ofs;

	if(size > top) {
		memcpy(line + top, hist_buf, size - top);
		size = top;
	}
	memcpy(line, hist_buf + ofs, size);

	line[size] = '\0';

	return 1;
}

/*
 * discard most recent item from history buffer
 */
void history_discard(void)
{
	if(hist_tail != hist_head)
		--hist_tail;
}

/*
 * return count of items in history buffer
 */
unsigned history_count(void)
{
	return hist_tail - hist_head;
}

/*
 * shell command - list history
 */
int cmnd_history(int opsz)
{
	char text[256];
	unsigned which;

	if(argc > 1)
		return E_ARGS_OVER;

	for(which = history_count(); which--;) {

		history_fetch(text, sizeof(text), which);
		printf("{%s}\n", text);
	}

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
