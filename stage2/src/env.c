/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

static union {
	struct {
		char		*item;
		unsigned	tag;
	} p[1];
	char t[1024];
} environ;

static unsigned nitems;

static int env_find(const char *name)
{
	unsigned indx, size;

	size = strlen(name);

	for(indx = 0; indx < nitems; ++indx)
		if(!strncasecmp(environ.p[indx].item, name, size) && environ.p[indx].item[size] == '=')
			return indx;

	return -1;
}

const char *env_get(const char *name)
{
	int indx;

	indx = env_find(name);
	if(indx >= 0)
		return environ.p[indx].item + strlen(name) + 1;

	return NULL;
}

static void env_remove_index(unsigned indx)
{
	unsigned move, fix;

	if(indx == --nitems)
		return;

	move = strlen(environ.p[indx].item) + 1;

	memmove(environ.p[nitems].item + move,
		environ.p[nitems].item,
		environ.p[indx].item - environ.p[nitems].item);

	for(fix = indx; fix < nitems; ++fix) {
		environ.p[fix].item = environ.p[fix + 1].item + move;
		environ.p[fix].tag = environ.p[fix + 1].tag;
	}
}

void env_remove_tag(unsigned tag)
{
	unsigned indx;

	for(indx = 0; indx < nitems;)
		if(environ.p[indx].tag == tag)
			env_remove_index(indx);
		else
			++indx;
}

int env_put(const char *name, const char *value, unsigned tag)
{
	void *ptr;
	int indx;

	indx = env_find(name);
	if(indx >= 0)
		env_remove_index(indx);

	if(!value)
		return indx >= 0;

	ptr = (nitems ? environ.p[nitems - 1].item : environ.t + sizeof(environ));

	ptr -= strlen(name) + 1 + strlen(value) + 1;
	if(ptr < (void *) &environ.p[nitems + 1])
		return 0;

	environ.p[nitems].item = ptr;
	environ.p[nitems++].tag = tag;

	strcpy(stpcpy(stpcpy(ptr, name), "="), value);

	return 1;
}

int cmnd_environ(int opsz)
{
	int indx;

	switch(argc) {

		case 1:

			for(indx = 0; indx < nitems; ++indx) {
				putstring_safe(environ.p[indx].item, -1);
				putchar('\n');
			}

			return E_NONE;

		case 2:

			if(!env_put(argv[1], NULL, 0)) {
				puts("no such variable");
				return E_UNSPEC;
			}

			return E_NONE;

		case 3:

			if(!env_put(argv[1], argv[2], VAR_OTHER)) {
				puts("out of variable space");
				return E_UNSPEC;
			}

			return E_NONE;
	}

	return E_ARGS_OVER;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
