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

static int cmnd_arguments(int);
static int cmnd_help(int);
static int cmnd_eval(int);
static int cmnd_reboot(int);
static int cmnd_nvflags(int);
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
	{ "serial",			cmnd_serial,		0,					"[rate | default]",										},
	{ "restrict",		cmnd_restrict,		0,					"[megabytes]",												},

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
 * toggle NV flags
 */
static int cmnd_nvflags(int opsz)
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
static int split_line(char *line)
{
	static char pool[384];

	unsigned indx, nout, size;
	const char *value;
	char delim, chr;
	char *name;
	int escp;

	nout = 0;
	argc = 0;

	do {

		for(; isspace(*line); ++line)
			;

		if(!*line)
			break;

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

					line[-1] = '\0';

					value = env_get(name);
					if(!value)
						return E_NO_SUCH_VAR;

					size = strlen(value);
					if(nout + size >= sizeof(pool))
						return E_ARGS_OVER;

					memcpy(pool + nout, value, size);
					nout += size;

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

					if(chr == '{')
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
	int opsz, ignore, error;
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
 * run script
 */
void script_exec(const char *run)
{
	script = run;
}

/*
 * loop reading command lines and dispatching
 */
void shell(void)
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
	};
	static char line[256], hist[256];
	int error, skip, escp;
	unsigned indx;

	assert(elements(argv) == elements(argsz));

	for(;;) {

		putstring("> ");

		indx = 0;

		if(script) {

			skip = 0;
			escp = 0;

			while(indx < sizeof(line) - 1) {

				line[indx] = *script++;

				if(!line[indx]) {
					script = NULL;
					break;
				}

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
		}

		if(!indx)
			line_edit(line, sizeof(line));

		putchar('\n');

		for(indx = 0; isspace(line[indx]); ++indx)
			;

		if(!line[indx])
			continue;

		if(!script && (!history_fetch(hist, sizeof(hist), 0) || strcmp(line, hist)))
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
#	define SCRIPT_SIGN			"#:CoLo:#"
#	define SCRIPT_SIGN_SZ		(sizeof(SCRIPT_SIGN) - 1)

	static char buf[2048];
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
