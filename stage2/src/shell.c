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
extern int cmnd_ping(int);
extern int cmnd_pci(int);
extern int cmnd_lcd(int);
extern int cmnd_environ(int);
extern int cmnd_boot(int);
extern int cmnd_nfs(int);
extern int cmnd_serial(int);
extern int cmnd_restrict(int);
extern int cmnd_menu(int);
extern int cmnd_script(int);
extern int cmnd_goto(int);
extern int cmnd_onerror(int);
extern int cmnd_exit(int);
extern int cmnd_abort(int);

static int cmnd_arguments(int);
static int cmnd_help(int);
static int cmnd_eval(int);
static int cmnd_reboot(int);
static int cmnd_nvflags(int);
static int cmnd_noop(int);
static int cmnd_sleep(int);

size_t argsz[MAX_CMND_ARGS];
char *argv[MAX_CMND_ARGS];
unsigned argc;

static struct
{
	const char	*name;
	int			(*func)(int);
	int			flags;
	const char	*parms;

} cmndtab[] = {

	{ "read",			cmnd_read,			FLAG_SIZED,		"[address]",												},
	{ "write",			cmnd_write,			FLAG_SIZED,		"[address] data",											},
	{ "dump",			cmnd_dump,			FLAG_SIZED,		"[address [count]]",										},
	{ "history",		cmnd_history,		0,					NULL,															},
	{ "evaluate",		cmnd_eval,			0,					"expression ...",											},
	{ "md5sum",			cmnd_md5sum,		0,					"[address size]",											},
	{ "keymap",			cmnd_keymap,		0,					"[keymap]",													},
	{ "?",				cmnd_help,			FLAG_NO_HELP,	NULL,															},
	{ "help",			cmnd_help,			0,					NULL,															},
	{ "download",		cmnd_srec,			0,					"[base-address]",											},
	{ "flash",			cmnd_flash,			0,					"[address size] target",								},
	{ "reboot",			cmnd_reboot,		0,					NULL,															},
	{ "image",			cmnd_heap,			0,					NULL,															},
	{ "showkey",		cmnd_keyshow,		0,					NULL,															},
	{ "unzip",			cmnd_unzip,			0,					NULL,															},
	{ "nvflags",		cmnd_nvflags,		0,					"[number ...]",											},
	{ "execute",		cmnd_execute,		0,					"[arguments ...]",										},
	{ "mount",			cmnd_mount,			0,					"[partition]",												},
	{ "ls",				cmnd_ls,				0,					"[path ...]",												},
	{ "cd",				cmnd_cd,				0,					"[path]",													},
	{ "load",			cmnd_load,			0,					"path [path]",												},
	{ "script",			cmnd_script,		0,					"[show]",													},
	{ "net",				cmnd_net,			0,					"[{address netmask [gateway]} | down]",			},
	{ "tftp",			cmnd_tftp,			0,					"host path [path]",										},
	{ "ping",			cmnd_ping,			0,					"host",														},
	{ "pci",				cmnd_pci,			FLAG_SIZED,		"[device[.function] register [value]]",			},
	{ "lcd",				cmnd_lcd,			0,					"[text [text]]",											},
	{ "variable",		cmnd_environ,		0,					"[name [value]]",											},
	{ "boot",			cmnd_boot,			0,					"[list | default] [option]",							},
	{ "nfs",				cmnd_nfs,			0,					"host root [path [path]]",								},
	{ "serial",			cmnd_serial,		0,					"[rate | default | on | off ]",						},
	{ "restrict",		cmnd_restrict,		0,					"[megabytes]",												},
	{ "goto",			cmnd_goto,			0,					"offset",													},
	{ "onfail",			cmnd_onerror,		0,					"offset",													},
	{ "abort",			cmnd_abort,			0,					NULL,															},
	{ "exit",			cmnd_exit,			0,					NULL,															},
	{ "select",			cmnd_menu,			0,					"title timeout option ...",							},
	{ "noop",			cmnd_noop,			0,					"[arguments ...]",										},
	{ "sleep",			cmnd_sleep,			0,					"sleep period",											},

#ifdef _DEBUG
	{ "arguments",		cmnd_arguments,	0,					"[arguments ...]",										},
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
 * shell command - sleep
 */
static int cmnd_sleep(int opsz)
{
	unsigned delay;
	char *ptr;

	if(argc < 2)
		return E_ARGS_UNDER;
	if(argc > 2)
		return E_ARGS_OVER;

	delay = strtoul(argv[1], &ptr, 10);
	if(ptr == argv[1] || *ptr)
		return E_BAD_EXPR;

	while(delay--) {

		if(BREAK()) {
			puts("aborted");
			return E_UNSPEC;
		}

		udelay(100 * 1000);
	}

	return E_NONE;
}

/*
 * shell command - noop
 */
static int cmnd_noop(int opsz)
{
	return E_NONE;
}

/*
 * shell command - toggle NV flags
 */
static int cmnd_nvflags(int opsz)
{
	static const char *msg[] = {
		"IDE LBA disabled",
		"IDE LBA48 disabled",
		"IDE timing disabled",
		"IDE slave enabled",
		"Console disabled",
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

	if(mask) {
		nv_store.flags ^= mask;
		nv_put();
	}

	for(indx = 0; indx < elements(msg); ++indx)
		printf("%x: %c %s\n", indx, (nv_store.flags & (1 << indx)) ? '*' : '.', msg[indx]);

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
 * split command line into argv[] array
 */
static int split_line(const char *iptr)
{
	static char pool[384];

	const char *varx, *name, *line;
	unsigned indx, nout;
	char delim, chr;
	int escp;

	line = iptr;
	varx = NULL;
	nout = 0;
	argc = 0;

	do {

		for(; isspace(*line); ++line)
			;

		if(!*line) {

			if(!varx)
				break;

			line = iptr;
			varx = NULL;

			for(; isspace(*line); ++line)
				;
		}

		if(argc == elements(argv) - 1 || nout >= sizeof(pool))
			return E_ARGS_OVER;

		argv[argc] = pool + nout;

		delim = ' ';
		escp = 0;
		name = NULL;

		for(;;) {

			chr = *line++;

			if(name) {

				if(chr == '}') {

					varx = env_get(name, line - name - 1);
					if(!varx)
						return E_NO_SUCH_VAR;

					iptr = line;
					line = varx;
					name = NULL;

					continue;

				} else

					if(!isalnum(chr) && chr != '-' && chr != '_') {
						line = name;
						name = NULL;
						escp = 1;
						chr = '{';
					}
			}

			if(!chr) {

				if(varx) {

					line = iptr;
					varx = NULL;

					/* if argument is empty skip extra spaces */

					if(argv[argc] == pool + nout)
						for(; isspace(*line); ++line)
							;

					continue;
				}

				if(escp) {
					pool[nout++] = '\\';
					if(nout >= sizeof(pool))
						return E_ARGS_OVER;
				}

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

			if(delim != '\'') {

				if(escp) {

					if(delim == '"')
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

					if(chr == '{' && !varx)
						name = line;
				}
			}

			if(!name) {
				pool[nout++] = chr;
				if(nout >= sizeof(pool))
					return E_ARGS_OVER;
			}
		}

		argsz[argc] = pool + nout - argv[argc];

		pool[nout++] = '\0';

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
	int opsz, error;
	unsigned indx;

	if(!argc)
		return E_ARGS_UNDER;

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

	error = E_INVALID_CMND;

	for(indx = 0; indx < elements(cmndtab); ++indx)
		if(!strncasecmp(argv[0], cmndtab[indx].name, argsz[0]) &&
			(!opsz || (cmndtab[indx].flags & FLAG_SIZED))) {

			error = cmndtab[indx].func(opsz);
			break;
		}

	return error;
}

/*
 * split line and execute
 */
int execute_line(const char *line, int *errptr)
{
	int quiet, error;

	while(isspace(*line))
		++line;

	quiet = (line[0] == '-');
	if(quiet)
		++line;

	error = split_line(line);

	if(error == E_NONE)
		error = execute();

	if(errptr)
		*errptr = error;

	if(error != E_NONE && quiet) {
		printf("(%s)\n", error_text(error));
		return E_NONE;
	}

	return error;
}

/*
 * translate error code to text
 */
const char *error_text(int error)
{
	static const char *msgs[] = {
		[E_INVALID_CMND]	= "unrecognised command",
		[E_ARGS_OVER]		= "too many arguments",
		[E_ARGS_UNDER]		= "missing arguments",
		[E_ARGS_COUNT]		= "incorrect number of arguments",
		[E_BAD_EXPR]		= "bad expression",
		[E_BAD_VALUE]		= "invalid value",
		[E_NO_SUCH_VAR]	= "no such variable",
		[E_NET_DOWN]		= "no interface",
		[E_NO_SCRIPT]		= "script only command",
	};
	static char buf[48];

	if((unsigned) error >= elements(msgs) || !msgs[error]) {
		sprintf(buf, "unknown error #%d", error);
		return buf;
	}

	return msgs[error];
}

/*
 * loop reading command lines and dispatching
 */
void shell(void)
{
	static char line[256], hist[256];
	unsigned indx;
	int error;

	assert(elements(argv) == elements(argsz));

	for(;;) {

		putstring("> ");

		line_edit(line, sizeof(line));

		putchar('\n');

		for(indx = 0; isspace(line[indx]); ++indx)
			;

		if(!line[indx])
			continue;

		if(!history_fetch(hist, sizeof(hist), 0) || strcmp(line, hist))
			history_add(line);

		error = execute_line(line, NULL);
		if(error != E_NONE && error != E_UNSPEC)
			puts(error_text(error));
	}
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
