/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

#define MAX_SCRIPT_DEPTH					10
#define MAX_SCRIPT_LINE						256
#define MAX_SCRIPT_MARKS					128
#define MAX_SCRIPT_TOTAL					16384

struct context
{
	struct context	*link;
	unsigned			depth;
	unsigned			etext;
	unsigned			mfree;
	unsigned			mcurr;
	unsigned			mjump;
};

static struct context root;
static struct context *current = &root;
static int cmdfail;

/*
 * relative goto for scripts
 */
static int script_goto(int ofs)
{
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
 * execute script
 */
int script_exec(const char *buf, int size)
{
	static char scripts[MAX_SCRIPT_TOTAL];
	static char *marks[MAX_SCRIPT_MARKS];
	static char line[MAX_SCRIPT_LINE];

	int err, xerr, skip, escp;
	unsigned indx, disp;
	struct context ctx;
	char *scrp;

	if(current->depth >= MAX_SCRIPT_DEPTH) {
		puts("script depth exceeded");
		return E_UNSPEC;
	}

	/* copy script to buffer */

	if(size < 0)
		size = strlen(buf);

	if(current->etext + size >= sizeof(scripts)) {
		puts("script too large");
		return E_UNSPEC;
	}

	scrp = scripts + current->etext;

	memcpy(scrp, buf, size);
	scrp[size++] = '\0';

	/* create new context */

	ctx.link = current;
	ctx.depth = current->depth + 1;
	ctx.etext = current->etext + size;
	ctx.mfree = current->mfree;
	ctx.mcurr = ctx.mfree;
	ctx.mjump = 0;

	current = &ctx;

	cmdfail = 0;

	for(err = E_NONE; *scrp;) {

		marks[ctx.mfree] = scrp;

		/* copy line from script */

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

			/* mark script position */

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

				err = execute_line(line + indx, &xerr);

				if(err != E_NONE) {
					if(err == E_EXIT_SCRIPT)
						err = E_NONE;
					else if(err != E_UNSPEC)
						puts(error_text(err));
					break;
				}

				/* save command status */

				cmdfail = (xerr != E_NONE);

				/* perform backwards jump */

				if(ctx.mjump && ctx.mjump <= ctx.mfree) {
					ctx.mcurr = ctx.mjump - 1;
					scrp = marks[ctx.mcurr];
				}
			}
		}
	}

	current = ctx.link;

	/* still waiting for a jump target ? */

	if(ctx.mcurr < ctx.mjump) {
		puts("forward jump out of range");
		err = E_UNSPEC;
	}

	printf(err == E_NONE ? "script exited <%d>\n" : "script aborted <%d>\n", ctx.depth);

	/* top level clean up */

	if(!current->depth)
		env_put("command-failed", NULL, 0);

	return err;
}

/*
 * shell command - exit
 */
int cmnd_exit(int opsz)
{
	if(argc > 1)
		return E_ARGS_OVER;

	if(!current->depth)
		return E_NO_SCRIPT;

	return E_EXIT_SCRIPT;
}

/*
 * shell command - abort
 */
int cmnd_abort(int opsz)
{
	if(argc > 1)
		return E_ARGS_OVER;

	if(!current->depth)
		return E_NO_SCRIPT;

	return E_UNSPEC;
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

/*
 * perform goto
 */
static int script_goto_text(const char *text)
{
	char *end;
	int ofs;

	if(!current->depth)
		return E_NO_SCRIPT;

	ofs = strtoul(text, &end, 10);

	if(end == text || (end[0] && end[1]))
		return E_BAD_VALUE;

	switch(end[0]) {

		case 'b':
		case 'B':
			if(ofs < 0)
				return E_BAD_VALUE;
			ofs = -ofs;
			break;

		case 'f':
		case 'F':
			if(ofs < 0)
				return E_BAD_VALUE;
			break;

		case '\0':
			break;

		default:
			return E_BAD_VALUE;
	}

	if(!script_goto(ofs))
		return E_UNSPEC;

	return E_NONE;
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

	return script_goto_text(argv[1]);
}

/*
 * shell command - onerror
 */
int cmnd_onerror(int opsz)
{
	if(argc < 2)
		return E_ARGS_UNDER;
	if(argc > 2)
		return E_ARGS_OVER;

	if(cmdfail)
		return script_goto_text(argv[1]);

	return E_NONE;
}


/* vi:set ts=3 sw=3 cin path=include,../include: */
