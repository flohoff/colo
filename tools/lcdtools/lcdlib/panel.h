/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _PANEL_H_
#define _PANEL_H_

#define BTN_CLEAR					(1 << 1)
#define BTN_LEFT					(1 << 2)
#define BTN_UP						(1 << 3)
#define BTN_DOWN					(1 << 4)
#define BTN_RIGHT					(1 << 5)
#define BTN_ENTER					(1 << 6)
#define BTN_SELECT				(1 << 7)

#define BTN_MASK					(BTN_CLEAR|BTN_LEFT|BTN_UP|BTN_DOWN|BTN_RIGHT|BTN_ENTER|BTN_SELECT)

#define LCD_WIDTH					16

extern const char *getapp(void);

extern int lcd_open(void);
extern void lcd_close(void);
extern void lcd_prog(unsigned, const void *);
extern void lcd_puts(unsigned, unsigned, unsigned, const char *);
extern void lcd_clear(void);
extern void lcd_curs_move(unsigned, unsigned);
extern void lcd_text(const char *, unsigned);
extern int btn_read(void);

#endif

/* vi:set ts=3 sw=3 cin: */
