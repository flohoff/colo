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
	unsigned			mfree;
	unsigned			mcurr;
	unsigned			mjump;
};

static char scripts[16384];
static char *marks[128];

static struct context root;
static struct context *current = &root;

/*
 * jump to mark in script
 */
int script_goto(const char *text)
{
	char *end;
	int ofs;

	ofs = strtoul(argv[1], &end, 10);

	if(end == argv[1] || *end) {
		puts(error_text(E_BAD_VALUE));
		return 0;
	}

	if(ofs) {

		if(ofs < 0) {

			ofs = -(ofs + 1);

			if(ofs >= current->mcurr - current->link->mfree) {
				puts("backward jump out of range");
				return 0;
			}

			current->mjump = current->mcurr - ofs;

		} else

			current->mjump = current->mcurr + ofs;
	}

	return 1;
}

/*
 * shell command - goto
 */
int cmnd_goto(int opsz)
{
	if(argc < 2)
		return E_ARGS_UNDER;
	if(argc > 2)
		return E_ARGS_OVER;

	if(!current->depth) {
		puts("script only command");
		return E_UNSPEC;
	}

	if(!script_goto(argv[1]))
		return E_UNSPEC;

	return E_NONE;
}

/*
 * execute script
 */
int script_exec(const char *buf, int size)
{
	static char line[256];
	unsigned indx, disp;
	int err, skip, escp;
	struct context ctx;
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
	ctx.mfree = current->mfree;
	ctx.mcurr = ctx.mfree;
	ctx.mjump = 0;

	current = &ctx;

	for(err = E_NONE; *scrp;) {

		marks[ctx.mfree] = scrp;

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

				if(ctx.mfree == ctx.link->mfree || marks[ctx.mfree] > marks[ctx.mfree - 1])
					if(++ctx.mfree >= elements(marks)) {
						puts("script mark table full");
						err = E_UNSPEC;
						break;
					}

				++ctx.mcurr;

				while(isspace(line[++indx]))
					;
			}

			/* skip execution if we're performing a forward jump */

			if(line[indx] && ctx.mcurr >= ctx.mjump) {

				printf("%d> ", ctx.depth);
				for(disp = indx; line[disp]; ++disp)
					putchar(line[disp]);
				putchar('\n');

				ctx.mjump = 0;

				/* recursion can occur here. it will trash the current line *
				 * and argc/argv etc. hopefully our caller is aware of this */

				err = execute_line(line + indx);

				if(err != E_NONE) {
					if(err != E_UNSPEC)
						puts(error_text(err));
					printf("script aborted <%d>\n", ctx.depth);
					break;
				}

				/* perform backwards jump */

				if(ctx.mjump && ctx.mjump <= ctx.mfree) {
					ctx.mcurr = ctx.mjump - 1;
					scrp = marks[ctx.mcurr];
				}
			}
		}
	}

	current = ctx.link;

	if(ctx.mcurr < ctx.mjump) {
		puts("forward jump out of range");
		return E_UNSPEC;
	}

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
