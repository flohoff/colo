/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

#define MAX_ENVIRONMENT_SIZE			2048

struct item {
	char		*item;
	unsigned	tag;
};

static union {
	struct item	p[1];
	char			t[MAX_ENVIRONMENT_SIZE];
} environ;

static unsigned nitems;

static int env_find(const char *name, int size)
{
	unsigned indx;

	for(indx = 0; indx < nitems; ++indx)
		if(!strncasecmp(environ.p[indx].item, name, size) && environ.p[indx].item[size] == '=')
			return indx;

	return -1;
}

const char *env_get(const char *name, int size)
{
	int indx;

	if(size < 0)
		size = strlen(name);

	indx = env_find(name, size);
	if(indx >= 0)
		return environ.p[indx].item + size + 1;

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

	indx = env_find(name, strlen(name));
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

static void env_dump(void)
{
	struct item env[nitems];
	struct item hold;
	unsigned indx;
	int swap;

	if(!nitems)
		return;

	memcpy(env, environ.p, sizeof(env));

	do {

		swap = 0;

		for(indx = 1; indx < nitems; ++indx)
			if(strcmp(env[indx - 1].item, env[indx].item) > 0) {

				hold = env[indx];
				env[indx] = env[indx - 1];
				env[indx - 1] = hold;

				swap = 1;
			}

	} while(swap);

	for(indx = 0; indx < nitems; ++indx) {
		putstring_safe(env[indx].item, -1);
		putchar('\n');
	}
}

int cmnd_environ(int opsz)
{
	int indx;

	switch(argc) {

		case 1:

			env_dump();

			return E_NONE;

		case 2:

			if(!env_put(argv[1], NULL, 0)) {
				puts("no such variable");
				return E_UNSPEC;
			}

			return E_NONE;

		case 3:

			for(indx = 0; argv[1][indx]; ++indx)
				if(!isalnum(argv[1][indx]) && argv[1][indx] != '-' && argv[1][indx] != '_') {
					puts("invalid variable name");
					return E_UNSPEC;
				}

			if(!env_put(argv[1], argv[2], VAR_OTHER)) {
				puts("out of variable space");
				return E_UNSPEC;
			}

			return E_NONE;
	}

	return E_ARGS_OVER;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
