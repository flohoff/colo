/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

/*
 * execute script
 */
int script_exec(const char *script)
{
	static char line[256];
	int err, skip, escp;
	unsigned indx;

	while(*script) {

		skip = 0;
		escp = 0;

		for(indx = 0; *script && indx < sizeof(line) - 1;) {

			line[indx] = *script++;

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
				putchar(line[indx++]);
		}

		line[indx] = '\0';

		for(indx = 0; isspace(line[indx]); ++indx)
			;
		
		if(line[indx]) {

			putchar('\n');

			err = execute_line(line);

			if(err != E_NONE) {
				puts(error_text(err));
				puts("script aborted");
				return err;
			}
		}
	}

	return E_NONE;
}

/*
 * shell command - script
 */
int cmnd_script(int opsz)
{
#	define SCRIPT_SIGN			"#:CoLo:#"
#	define SCRIPT_SIGN_SZ		(sizeof(SCRIPT_SIGN) - 1)

	static char buf[4096];
	unsigned indx;
	size_t size;
	void *ptr;

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

	if(size >= sizeof(buf)) {
		puts("script too large");
		return E_UNSPEC;
	}

	memcpy(buf, ptr, size);
	
	buf[size] = '\0';

	if(argc == 1) {
		script_exec(buf);
		return E_NONE;
	}

	for(indx = 0; buf[indx]; ++indx) {
		if(buf[indx] == '\r' && buf[indx + 1] == '\n')
			++indx;
		putchar((buf[indx] == '\n' || isprint(buf[indx])) ? buf[indx] : '?');
	}

	if(indx && buf[indx - 1] != '\n')
		putchar('\n');

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
