/*
 * (C) BitBox Ltd 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage2/src/keymap.c,v 1.2 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"
#include "cpu.h"
#include "keymap.h"

#define INTER_KEY_TIMEOUT				(CP0_COUNT_RATE / 20)
#define MAX_BIND_CHARS					16

struct keybind_t
{
	const char	*keys;
	int			code;
};

struct keymap_t
{
	const char			*name;
	struct keybind_t	*map;
};

static const struct keybind_t minicom_vt102_bind[] =
{
	{ "\033[A",		KEY_HISTORY_PREV	},
	{ "\033[B",		KEY_HISTORY_NEXT	},
	{ "\033[C",		KEY_CURSOR_RIGHT	},
	{ "\033[D",		KEY_CURSOR_LEFT	},
	{ "\033[1~",	KEY_HOME				},
	{ "\033[3~",	KEY_DELETE			},
	{ "\033[4~",	KEY_END				},
	{ "\033[5~",	KEY_WORD_LEFT		},
	{ "\033[6~",	KEY_WORD_RIGHT		},
	{ "\b",			KEY_BACKSPACE		},
	{ "\t",			KEY_HISTORY_MATCH	},
	{ "\r",			KEY_ENTER			},
	{ "\025",		KEY_CLEAR			},
	{ NULL,			0						}
};

static const struct keybind_t hyperterminal_ansi_bind[] =
{
	{ "\033[A",		KEY_HISTORY_PREV	},
	{ "\033[B",		KEY_HISTORY_NEXT	},
	{ "\033[C",		KEY_CURSOR_RIGHT	},
	{ "\033[D",		KEY_CURSOR_LEFT	},
	{ "\033[H",		KEY_HOME				},
	{ "\033[K",		KEY_END				},
	{ "\02",			KEY_WORD_LEFT		},
	{ "\06",			KEY_WORD_RIGHT		},
	{ "\b",			KEY_BACKSPACE		},
	{ "\t",			KEY_HISTORY_MATCH	},
	{ "\r",			KEY_ENTER			},
	{ "\025",		KEY_CLEAR			},
	{ NULL,			0						}
};

static const struct keybind_t teraterm_vt100_bind[] =
{
	{ "\033[A",		KEY_HISTORY_PREV	},
	{ "\033[B",		KEY_HISTORY_NEXT	},
	{ "\033[C",		KEY_CURSOR_RIGHT	},
	{ "\033[D",		KEY_CURSOR_LEFT	},
	{ "\033[2~",	KEY_HOME				},
	{ "\033[5~",	KEY_END				},
	{ "\033[3~",	KEY_WORD_LEFT		},
	{ "\033[6~",	KEY_WORD_RIGHT		},
	{ "\033[4~",	KEY_DELETE			},
	{ "\b",			KEY_BACKSPACE		},
	{ "\t",			KEY_HISTORY_MATCH	},
	{ "\r",			KEY_ENTER			},
	{ "\025",		KEY_CLEAR			},
	{ NULL,			0						}
};

static const struct keymap_t keymaps[] =
{
	{ "minicom-vt102",		(struct keybind_t *) minicom_vt102_bind		},
	{ "hyperterminal-ansi",	(struct keybind_t *) hyperterminal_ansi_bind	},
	{ "teraterm-vt100",		(struct keybind_t *) teraterm_vt100_bind		},
};

static const struct keymap_t *keymap = &keymaps[0];

int kgetch(void)
{
	static char buf[MAX_BIND_CHARS];
	static unsigned fill;
	unsigned long mark;
	unsigned indx;
	char chr;

	if(!fill)
		buf[fill++] = getch();

	for(;;) {

		for(indx = 0; keymap->map[indx].keys && memcmp(buf, keymap->map[indx].keys, fill); ++indx)
			;

		if(!keymap->map[indx].keys)
			break;

		if(!keymap->map[indx].keys[fill]) {
			fill = 0;
			return keymap->map[indx].code;
		}

		for(mark = MFC0(CP0_COUNT); !kbhit() && MFC0(CP0_COUNT) - mark < INTER_KEY_TIMEOUT;)
			;

		if(!kbhit())
			break;

		buf[fill++] = getch();
	}

	chr = buf[0];
	--fill;
	for(indx = 0; indx < fill; ++indx)
		buf[indx] = buf[indx + 1];

	return chr;
}

int cmnd_keyshow(int opsz)
{
	static char buf[MAX_BIND_CHARS];
	unsigned long mark;
	unsigned indx;

	if(argc > 1)
		return E_ARGS_OVER;

	indx = 0;

	do {

		if(indx == sizeof(buf)) {
			puts("key sequence too long");
			break;
		}

		buf[indx++] = getch();

		for(mark = MFC0(CP0_COUNT); !kbhit() && MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE;)
			;

	} while(kbhit());

	putchar('"');
	putstring_safe(buf, indx);
	puts("\"");

	return E_SUCCESS;
}

int cmnd_keymap(int opsz)
{
	unsigned indx;

	if(argc > 2)
		return E_ARGS_OVER;

	if(argc < 2) {

		for(indx = elements(keymaps); indx--;) {

			putchar(keymap == &keymaps[indx] ? '*' : ' ');
			putchar(' ');
			puts(keymaps[indx].name);
		}

		return E_SUCCESS;
	}

	for(indx = elements(keymaps); indx--;)

		if(!strncasecmp(argv[1], keymaps[indx].name, argsz[1])) {

			keymap = &keymaps[indx];
			return E_SUCCESS;
		}

	puts("unrecognised keymap");

	return E_SUCCESS;
}

/* vi:set ts=3 sw=3 cin: */
