/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _CLIB_H_
#define _CLIB_H_

#define __STR(x)				#x
#define _STR(x)				__STR(x)

#define yield()

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned				uint32_t;

typedef unsigned				size_t;

/* libmem.c */

extern void *memcpy(void *, const void *, size_t);
extern void *memset(void *, int, size_t);
extern int memcmp(const void *, const void *, size_t);

/* serial.c */

extern void serial_init(void);
extern void putstring(const char *);
extern void puts(const char *);
extern void drain(void);
extern void putchar(int);

#endif

/* vi:set ts=3 sw=3 cin path=include,../include: */
