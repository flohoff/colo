/*
 * (C) P.Horton 2004
 *
 * $Header: /export1/cvs/cobalt-boot/boot-loader/stage1/include/lib.h,v 1.2 2004/02/15 12:45:18 pdh Exp $
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _CLIB_H_
#define _CLIB_H_

#define NULL					((void *) 0)

#define __STR(x)				#x
#define _STR(x)				__STR(x)

#define DIE()					do{*(volatile int *)0=0;}while(0)
#define DIE_ON(x)				do{if(x)DIE();}while(0)

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned				uint32_t;

typedef unsigned				size_t;

/* lcd.c */

extern void lcd_init(void);
extern void lcd_line(int, const char *);

/* dram.c */

extern size_t *dram_init(size_t *);

/* main.c */

extern unsigned switches;
extern size_t mem_bank[];

extern char *to_decimal(char *, unsigned);
extern char *to_hex(char *, unsigned, unsigned);

/* init.c */

extern void __attribute__((noreturn)) fatal(void);
extern void __attribute__((noreturn)) exception(unsigned long);

/* fast.c */

extern void * __attribute__((section(".data"))) _memcpy_w(void *, const void *, size_t);
extern void * __attribute__((section(".data"))) _memset_w(void *, int, size_t);

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
