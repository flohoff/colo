/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "galileo.h"

#define FLAG_SIZED				(1 << 0)
#define FLAG_NO_HELP				(1 << 1)

extern int cmnd_execute(int);
extern int cmnd_mount(int);
extern int cmnd_ls(int);
extern int cmnd_cd(int);
extern int cmnd_load(int);
extern int cmnd_read(int);
extern int cmnd_write(int);
extern int cmnd_dump(int);
extern int cmnd_history(int);
extern int cmnd_md5sum(int);
extern int cmnd_srec(int);
extern int cmnd_keymap(int);
extern int cmnd_keyshow(int);
extern int cmnd_flash(int);
extern int cmnd_heap(int);
extern int cmnd_unzip(int);
extern int cmnd_net(int);
extern int cmnd_tftp(int);

static int cmnd_arguments(int);
static int cmnd_help(int);
static int cmnd_eval(int);
static int cmnd_reboot(int);
static int cmnd_dflags(int);
static int cmnd_script(int);

static const char *script;
size_t argsz[32];
char *argv[32];
unsigned argc;

static struct
{
	const char	*name;
	int			(*func)(int);
	int			flags;
	const char	*parms;

} cmndtab[] = {

	{ "read",			cmnd_read,			FLAG_SIZED,		"[address]",										},
	{ "write",			cmnd_write,			FLAG_SIZED,		"[address] data",									},
	{ "dump",			cmnd_dump,			FLAG_SIZED,		"[address [count]]",								},
	{ "history",		cmnd_history,		0,					NULL,													},
	{ "evaluate",		cmnd_eval,			0,					"expression ...",									},
	{ "md5sum",			cmnd_md5sum,		0,					"[address size]",									},
	{ "keymap",			cmnd_keymap,		0,					"[keymap]",											},
	{ "?",				cmnd_help,			FLAG_NO_HELP,	NULL,													},
	{ "help",			cmnd_help,			0,					NULL,													},
	{ "download",		cmnd_srec,			0,					"[base-address]",									},
	{ "flash",			cmnd_flash,			0,					"[address size] target",						},
	{ "reboot",			cmnd_reboot,		0,					NULL,													},
	{ "image",			cmnd_heap,			0,					NULL,													},
	{ "showkey",		cmnd_keyshow,		0,					NULL,													},
	{ "unzip",			cmnd_unzip,			0,					NULL,													},
	{ "dflags",			cmnd_dflags,		0,					"[number ...]",									},
	{ "execute",		cmnd_execute,		0,					"[arguments ...]",								},
	{ "mount",			cmnd_mount,			0,					"[partition]",										},
	{ "ls",				cmnd_ls,				0,					"[path ...]",										},
	{ "cd",				cmnd_cd,				0,					"[path]",											},
	{ "load",			cmnd_load,			0,					"path [path]",										},
	{ "script",			cmnd_script,		0,					"path",												},
	{ "net",				cmnd_net,			0,					"[{address netmask [gateway]} | down]",	},
	{ "tftp",			cmnd_tftp,			0,					"host path [path]",								},

#ifdef _DEBUG
	{ "arguments",		cmnd_arguments,	0,					"[arguments ...]",								},
#endif
};

#ifdef _DEBUG

/*
 * shell command - list command arguments
 */
static int cmnd_arguments(int opsz)
{
	unsigned indx;

	for(indx = 0; indx < argc; ++indx) {

		putchar('{');
		putstring_safe(argv[indx], argsz[indx]);
		puts("}");
	}
	
	return E_NONE;
}

#endif

/*
 * toggle debug flags
 */
static int cmnd_dflags(int opsz)
{
	static const char *msg[] = {
		"IDE LBA disabled",
		"IDE LBA48 disabled",
		"IDE timing disabled",
		"IDE slave enabled",
	};
	unsigned indx, mask;
	unsigned long bit;
	char *ptr;

	mask = 0;

	for(indx = 1; indx < argc; ++indx) {

		bit = evaluate(argv[indx], &ptr);
		if(*ptr)
			return E_BAD_EXPR;
		if(bit > 31)
			return E_BAD_VALUE;

		mask |= 1 << bit;
	}

	debug_flags ^= mask;

	for(indx = 0; indx < elements(msg); ++indx)
		printf("%2u: %c %s\n", indx, (debug_flags & (1 << indx)) ? '*' : '.', msg[indx]);

	return E_NONE;
}

/*
 * shell command - reboot
 */
static int cmnd_reboot(int opsz)
{
	unsigned leds;

	drain();

	udelay(50000);

	*(volatile uint8_t *) BRDG_NCS0_BASE = (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0);

	for(leds = (1 << 1) | (1 << 0);; leds ^= (1 << 1) | (1 << 0)) {

		*(volatile uint8_t *) BRDG_NCS0_BASE = leds;
		udelay(400000);
	}
}

/*
 * shell command - help
 */
static int cmnd_help(int opsz)
{
#	define SIZE_SPEC		"[.{b|h|w}]"

	unsigned indx, width;
	size_t size;

	if(argc > 1)
		return E_ARGS_OVER;

	width = 0;

	for(indx = 0; indx < elements(cmndtab); ++indx) {
		if(!(cmndtab[indx].flags & FLAG_NO_HELP)) {
			size = strlen(cmndtab[indx].name);
			if(cmndtab[indx].flags & FLAG_SIZED)
				size += sizeof(SIZE_SPEC) - 1;
			if(size > width)
				width = size;
		}
	}

	width += 2;

	for(indx = 0; indx < elements(cmndtab); ++indx) {
		if(!(cmndtab[indx].flags & FLAG_NO_HELP))
			printf("%s%-*s%s\n",
				cmndtab[indx].name,
				width - strlen(cmndtab[indx].name),
				(cmndtab[indx].flags & FLAG_SIZED) ? SIZE_SPEC : "",
				cmndtab[indx].parms ? cmndtab[indx].parms : "");
	}

	return E_NONE;
}

/*
 * shell command - evaluate
 */
static int cmnd_eval(int opsz)
{
	unsigned long value;
	unsigned indx, size;
	char text[256];
	char *end;

	size = 0;

	for(indx = 1; indx < argc; ++indx) {

		if(argsz[indx] + 1 >= sizeof(text) - size)
			return E_ARGS_OVER;

		if(size)
			text[size++] = ' ';
		memcpy(&text[size], argv[indx], argsz[indx]);
		size += argsz[indx];
	}

	if(!size)
		return E_ARGS_UNDER;

	text[size] = '\0';

#ifdef _DEBUG

	putchar('{');
	putstring_safe(text, size);
	puts("}");

#endif

	value = evaluate(text, &end);
	if(*end)
		return E_BAD_EXPR;

	printf((long) value < 0 ? "%08lx %lut (-%08lx -%lut)\n" : "%08lx %lut\n", value, value, -value, -value);

	return E_NONE;
}

/*
 * add argument to command line
 */
int argv_add(const char *str)
{
	if(argc == elements(argv) - 1)
		return 0;

	argv[argc] = (char *) str;
	argsz[argc++] = strlen(str);
	argv[argc] = NULL;

	return 1;
}

/*
 * split command line into argv[] array
 */
static int split_line(char *line)
{
	char delim, chr;
	unsigned indx;
	char *pool;
	int escp;

	pool = line;
	argc = 0;

	do {

		for(; isspace(*line); ++line)
			;

		if(!*line)
			break;

		if(argc == elements(argv) - 1)
			return E_ARGS_OVER;

		argv[argc] = pool;

		for(escp = 0, delim = ' ';;)
		{
			chr = *line++;

			if(!chr) {
				if(escp)
					*pool++ = '\\';
				break;
			}

			if(!escp && chr == delim) {
				if(delim == ' ')
					break;
				delim = ' ';
				continue;
			}

			if(delim == ' ' && (chr == '\'' || chr == '"')) {
				delim = chr;
				continue;
			}

			if(delim == '"') {

				if(escp) {

					switch(chr)
					{
						case 'a': chr = '\a'; break;
						case 'b': chr = '\b'; break;
						case 'f': chr = '\f'; break;
						case 'n': chr = '\n'; break;
						case 'r': chr = '\r'; break;
						case 't': chr = '\t'; break;
						case 'v': chr = '\v'; break;
						case 'e': chr = 27; break;

						case '0':
							chr = 0;
							for(indx = 0; indx < 3 && line[indx] >= '0' && line[indx] <= '7'; ++indx)
								chr = (chr << 3) | (line[indx] & 0xf);
							line += indx;
							break;

						case 'x':
							chr = 0;
							for(indx = 0; indx < 2 && isxdigit(line[indx]); ++indx) {
								chr = (chr << 4) | (line[indx] & 0xf);
								if(line[indx] >= 'A')
									chr += 9;
							}
							line += indx;
					}

					escp = 0;

				} else {

					if(chr == '\\') {
						escp = 1;
						continue;
					}
				}
			}

			*pool++ = chr;
		}

		argsz[argc] = pool - argv[argc];

		*pool++ = '\0';

		++argc;

	} while(chr);

	argv[argc] = NULL;

	return E_NONE;
}

/*
 * execute command in argv[]
 */
static int execute(void)
{
	int opsz, ignore, error;
	unsigned indx;

	if(!argc)
		return E_ARGS_UNDER;

	if(argsz[0] && argv[0][0] == '#')
		return E_NONE;

	/* strip size suffix from command */

	opsz = 0;

	if(argsz[0] >= 2 && argv[0][argsz[0] - 2] == '.') {

		switch(argv[0][argsz[0] - 1]) {
			case 'b': case 'B': opsz = 1; break;
			case 'h': case 'H': opsz = 2; break;
			case 'w': case 'W': opsz = 4;
		}

		if(opsz) {
			argsz[0] -= 2;
			argv[0][argsz[0]] = '\0';
		}
	}

	/* find and call command */

	ignore = (argsz[0] && argv[0][0] == '-');

	for(indx = 0; indx < elements(cmndtab); ++indx)
		if(!strncasecmp(argv[0] + ignore, cmndtab[indx].name, argsz[0] - ignore) &&
			(!opsz || (cmndtab[indx].flags & FLAG_SIZED))) {

			error = cmndtab[indx].func(opsz);
			return ignore ? E_NONE : error;
		}

	return E_INVALID_CMND;
}

/*
 * loop reading command lines and dispatching
 */
void shell(const char *run)
{
	static const char *msgs[] = {
		[E_INVALID_CMND]	= "unrecognised command",
		[E_ARGS_OVER]		= "too many arguments",
		[E_ARGS_UNDER]		= "missing arguments",
		[E_ARGS_COUNT]		= "incorrect number of arguments",
		[E_BAD_EXPR]		= "bad expression",
		[E_BAD_VALUE]		= "invalid value",
	};
	static char line[256], hist[256];
	unsigned indx;
	int error;

	assert(elements(argv) == elements(argsz));

	for(script = run;;) {

		putstring("> ");

		if(script) {

			/* fetch line from script */

			for(indx = 0; indx < sizeof(line) - 1;) {

				line[indx] = *script++;

				if(!line[indx]) {
					script = NULL;
					break;
				}

				if(indx && (line[indx] == '\n' || line[indx] == '\r'))
					break;

				if(line[indx] >= ' ' && line[indx] < '~')
					putchar(line[indx++]);
			}

			line[indx] = '\0';

		} else

			line_edit(line, sizeof(line));

		putchar('\n');

		/* add non-blank non-repeated lines to history */

		for(indx = 0; isspace(line[indx]); ++indx)
			;

		if(line[indx] && (!history_fetch(hist, sizeof(hist), 0) || strcmp(line, hist)))
			history_add(line);

		error = split_line(line);

		if(error == E_NONE && argc)
			error = execute();

		if(error != E_NONE) {

			if(error != E_UNSPEC) {
				if(error >= elements(msgs) || !msgs[error])
					printf("unkown error #%d\n", error);
				else
					puts(msgs[error]);
			}

			if(script) {
				script = NULL;
				puts("script aborted");
			}
		}
	}
}

/*
 * shell command - script
 */
static int cmnd_script(int opsz)
{
	static char buf[1024];
	unsigned long size;
	void *hdl;

	if(argc < 2)
		return E_ARGS_UNDER;

	if(argc > 2)
		return E_ARGS_OVER;

	hdl = file_open(argv[1], &size);
	if(!hdl)
		return E_UNSPEC;

	if(size > sizeof(buf) - 1) {
		puts("script file too large");
		return E_UNSPEC;
	}

	if(!file_load(hdl, buf, size))
		return E_UNSPEC;

	buf[size] = '\0';

	script = buf;

	return E_NONE;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
