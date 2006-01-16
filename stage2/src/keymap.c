/*
 * (C) BitBox Ltd 2004
 *
 * $Id$
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

static const struct keybind_t screen_vt100_bind[] =
{
	{ "\033[A",		KEY_HISTORY_PREV	},
	{ "\033[B",		KEY_HISTORY_NEXT	},
	{ "\033[C",		KEY_CURSOR_RIGHT	},
	{ "\033[D",		KEY_CURSOR_LEFT	},
	{ "\033[1~",	KEY_HOME,			},
	{ "\033[4~",	KEY_END,				},
	{ "\033[5~",	KEY_WORD_LEFT,		},
	{ "\033[6~",	KEY_WORD_RIGHT,	},
	{ "\033[3~",	KEY_DELETE,			},
	{ "\177",		KEY_BACKSPACE,		},
	{ "\t",			KEY_HISTORY_MATCH	},
	{ "\r",			KEY_ENTER			},
	{ "\025",		KEY_CLEAR			},
	{ NULL,			0						}
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
	{ "screen-vt100",			(struct keybind_t *) screen_vt100_bind			},
};

int kgetch(void)
{
	static char buf[MAX_BIND_CHARS];
	const struct keybind_t *bind;
	static unsigned fill;
	unsigned long mark;
	unsigned indx;
	char chr;

	bind = keymaps[0].map;
	if(nv_store.keymap < elements(keymaps))
		bind = keymaps[nv_store.keymap].map;

	if(!fill)
		buf[fill++] = getch();

	for(;;) {

		for(indx = 0; bind[indx].keys && memcmp(buf, bind[indx].keys, fill); ++indx)
			;

		if(!bind[indx].keys)
			break;

		if(!bind[indx].keys[fill]) {
			fill = 0;
			return bind[indx].code;
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
	unsigned indx, key;

	if(argc > 1)
		return E_ARGS_OVER;

	indx = 0;

	do {

		key = getch();
		if(indx < sizeof(buf)) 
			buf[indx] = key;
		++indx;

		for(mark = MFC0(CP0_COUNT); !kbhit() && MFC0(CP0_COUNT) - mark < CP0_COUNT_RATE;)
			;

	} while(kbhit());

	if(indx > sizeof(buf)) {
		puts("key sequence too long");
		return E_UNSPEC;
	}

	putchar('"');
	putstring_safe(buf, indx);
	puts("\"");

	return E_NONE;
}

int cmnd_keymap(int opsz)
{
	unsigned indx;

	if(argc > 2)
		return E_ARGS_OVER;

	if(argc < 2) {

		for(indx = elements(keymaps); indx--;) {

			putchar(nv_store.keymap == indx ? '*' : ' ');
			putchar(' ');
			puts(keymaps[indx].name);
		}

		return E_NONE;
	}

	for(indx = elements(keymaps); indx--;)

		if(!strncasecmp(argv[1], keymaps[indx].name, argsz[1])) {

			if(indx != nv_store.keymap) {
				nv_store.keymap = indx;
				nv_put();
			}

			return E_NONE;
		}

	puts("unrecognised keymap");

	return E_UNSPEC;
}

/* vi:set ts=3 sw=3 cin: */
