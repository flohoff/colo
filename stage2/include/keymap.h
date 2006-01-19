/*
 * (C) BitBox Ltd 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _KEYMAP_H_
#define _KEYMAP_H_

#define KEY_CURSOR_LEFT					0x100
#define KEY_CURSOR_RIGHT				0x101
#define KEY_WORD_LEFT					0x102
#define KEY_WORD_RIGHT					0x103
#define KEY_HOME							0x104
#define KEY_END							0x105
#define KEY_BACKSPACE					0x106
#define KEY_DELETE						0x107
#define KEY_ENTER							0x108
#define KEY_CLEAR							0x109
#define KEY_HISTORY_PREV				0x10a
#define KEY_HISTORY_NEXT				0x10b
#define KEY_HISTORY_MATCH				0x10c
#define KEY_DELETE_WORD					0x10d

extern int kgetch(void);

#endif

/* vi:set ts=3 sw=3 cin: */
