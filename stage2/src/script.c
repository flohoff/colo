/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

#define MAX_SCRIPT_DEPTH					10

struct context
{
	struct context	*link;
	unsigned			depth;
	unsigned			etext;
};

static char scripts[16384];

static struct context root =
{
	.link		= NULL,
	.depth	= 0,
	.etext	= 0,
};

static struct context *current = &root;

/*
 * execute script
 */
int script_exec(const char *buf, int size)
{
	static char line[256];
	int err, skip, escp;
	struct context ctx;
	unsigned indx;
	char *scrp;

	if(current->depth >= MAX_SCRIPT_DEPTH) {
		puts("script depth exceeded");
		return E_UNSPEC;
	}

	if(size < 0)
		size = strlen(buf);

	if(current->etext + size >= sizeof(scripts)) {
		puts("script too large");
		return E_UNSPEC;
	}

	scrp = scripts + current->etext;

	memcpy(scrp, buf, size);
	scrp[size++] = '\0';

	ctx.link = current;
	ctx.depth = current->depth + 1;
	ctx.etext = current->etext + size;

	current = &ctx;

	for(err = E_NONE; *scrp;) {

		skip = 0;
		escp = 0;

		for(indx = 0; *scrp && indx < sizeof(line) - 1;) {

			line[indx] = *scrp++;

			if(line[indx] == '\n' || line[indx] == '\r') {
				if(indx)
					break;
				skip = 0;
				escp = 0;
			}

			if(escp)
				escp = 0;
			else {
				if(line[indx] == '#')
					skip = 1;
				if(line[indx] == '\\')
					escp = 1;
			}

			if(!skip && (indx || !isspace(line[indx])) && isprint(line[indx]))
				++indx;
		}

		line[indx] = '\0';

		if(indx) {

			indx = 0;

			if(line[0] == '@') {

				while(isspace(line[++indx]))
					;
			}

			if(line[indx]) {

				/* show what we're about to do */

				printf("%d> ", ctx.depth);
				while(line[indx])
					putchar(line[indx++]);
				putchar('\n');

				/* recursion can occur here. it will trash the  *
				 * current line and argc/argv etc but who cares */

				err = execute_line(line);

				if(err != E_NONE) {
					if(err != E_UNSPEC)
						puts(error_text(err));
					printf("script aborted <%d>\n", ctx.depth);
					break;
				}
			}
		}
	}

	current = ctx.link;

	return err;
}

/*
 * shell command - script
 */
int cmnd_script(int opsz)
{
#	define SCRIPT_SIGN			"#:CoLo:#"
#	define SCRIPT_SIGN_SZ		(sizeof(SCRIPT_SIGN) - 1)

	const char *ptr;
	unsigned indx;
	size_t size;

	if(argc > 2)
		return E_ARGS_OVER;

	if(argc > 1 && strncasecmp(argv[1], "show", strlen(argv[1]))) {
		puts("invalid argument");
		return E_UNSPEC;
	}

	ptr = heap_image(&size);

	if(!size) {
		puts("no script loaded");
		return E_UNSPEC;
	}

	if(size < SCRIPT_SIGN_SZ || strncasecmp(ptr, SCRIPT_SIGN, SCRIPT_SIGN_SZ)) {
		puts("missing script header");
		return E_UNSPEC;
	}

	if(argc == 1) {
		script_exec(ptr, size);
		return E_NONE;
	}

	for(indx = 0; indx < size; ++indx) {
		if(ptr[indx] == '\r' && size - indx > 1 && ptr[indx + 1] == '\n')
			++indx;
		putchar((ptr[indx] == '\n' || isprint(ptr[indx])) ? ptr[indx] : '?');
	}

	if(indx && ptr[indx - 1] != '\n')
		putchar('\n');

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
