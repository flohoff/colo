/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

#define PFF_JUSTIFY_LEFT		(1 << 0)
#define PFF_ZEROES				(1 << 1)
#define PFF_LONG					(1 << 2)
#define PFF_SHORT					(1 << 3)
#define PFF_SIGNED				(1 << 4)
#define PFF_NOCAPS				(1 << 5)
#define PFF_PREFIX				(1 << 6)
#define PFF_SIGN_POSITIVE		(1 << 7)
#define PFF_SIGN_BLANK			(1 << 8)
#define PFF_NEGATIVE				(1 << 9)
#define PFF_LONG_LONG			(1 << 10)

#define WRITE_CHAR(p,c,n)		do{for(;(n);--(n))*(p)++=(c);}while(0)

int vsprintf(char *buf, const char *fmt, va_list arg)
{
	const char *spc, *ptr, *beg;
	unsigned flg, len, rdx, ndg;
	unsigned long val;
	char dig[11];
	int wid, prc;

	for(beg = buf; (spc = strchr(fmt, '%')) != NULL && spc[1] != '\0'; fmt = spc) {

		memcpy(buf, fmt, spc - fmt);
		buf += spc++ - fmt;

		if(*spc == '%') {
			++spc;
			*buf++ = '%';
			continue;
		}

		for(flg = 0;;) {
			switch(*spc) {

				case '0':
					flg |= PFF_ZEROES;
					++spc;
					continue;

				case '-':
					flg |= PFF_JUSTIFY_LEFT;
					++spc;
					continue;

				case '#':
					flg |= PFF_PREFIX;
					++spc;
					continue;

				case '+':
					flg |= PFF_SIGN_POSITIVE;
					++spc;
					continue;

				case ' ':
					flg |= PFF_SIGN_BLANK;
					++spc;
					continue;
			}
			break;
		}

		if(*spc == '*') {
			++spc;
			if((wid = va_arg(arg, int)) < 0) {
				wid = 0 - wid;
				flg |= PFF_JUSTIFY_LEFT;
			}
		} else
			for(wid = 0; *spc >= '0' && *spc <= '9'; wid = wid * 10 + *spc++ - '0')
				;

		if(*spc == '.') {
			if(*++spc == '*') {
				++spc;
				if((prc = va_arg(arg, int)) < 0)
					prc = 0;
			} else {
				prc = 0;
				if(*spc == '-')
					while(*++spc >= '0' && *spc <= '9')
						;
				else
					for(; *spc >= '0' && *spc <= '9'; prc = prc * 10 + *spc++ - '0')
						;
			}
		} else
			prc = -1;

		if(*spc == 'l') {
			if(*++spc == 'l') {
				++spc;
				flg |= PFF_LONG_LONG;	/* not currently used */
			} else
				flg |= PFF_LONG;
		} else if(*spc == 'h') {
			++spc;
			flg |= PFF_SHORT;				/* not currently used */
		}

		switch(*spc) {

			case 'n':
				*va_arg(arg, int *) = buf - beg;
				continue;

			case 'c':
				wid = (wid > 1) ? wid - 1 : 0;
				if(wid && !(flg & PFF_JUSTIFY_LEFT))
					WRITE_CHAR(buf, ' ', wid);
				*buf++ = (flg & PFF_LONG) ? va_arg(arg, long) : va_arg(arg, int);
				break;

			case 's':
				ptr = va_arg(arg, const char *);
				len = strlen(ptr);
				if(prc >= 0 && len > prc)
					len = prc;
				wid = (wid > len) ? wid - len : 0;
				if(wid && !(flg & PFF_JUSTIFY_LEFT))
					WRITE_CHAR(buf, ' ', wid);
				memcpy(buf, ptr, len);
				buf += len;
				break;

			default:
				switch(*spc) {

					case 'd':
					case 'i':
						flg |= PFF_SIGNED;
						/* */

					case 'u':
						rdx = 10;
						break;

					case 'o':
						rdx = 8;
						break;

					case 'p':
						flg |= (sizeof(void *) == sizeof(long)) ? PFF_LONG | PFF_PREFIX : PFF_PREFIX;
						/* */

					case 'x':
						flg |= PFF_NOCAPS;
						/* */

					case 'X':
						rdx = 16;
						break;

					default:
						continue;
				}

				val = (flg & PFF_LONG) ? va_arg(arg, unsigned long) : va_arg(arg, unsigned int);

				if(flg & PFF_SIGNED) {
					if(flg & PFF_LONG) {
						if((long) val < 0) {
							val = 0 - val;
							flg |= PFF_NEGATIVE;
						}
					} else if((int) val < 0) {
						val = 0 - (int) val;
						flg |= PFF_NEGATIVE;
					}
				}

				ndg = 0;
				do {
					dig[ndg] = val % rdx + '0';
					if(dig[ndg] > '9')
						dig[ndg] += ((flg & PFF_NOCAPS) ? 'a' : 'A') - ('9' + 1);
					val /= rdx;
					++ndg;
				} while(val);

				len = ndg;
				if((int) len < prc)
					len = prc;

				if((flg & (PFF_NEGATIVE | PFF_SIGN_BLANK | PFF_SIGN_POSITIVE)))
					++len;
				if(flg & PFF_PREFIX) {
					if(rdx == 16)
						len += 2;
					else if(rdx == 8 && dig[ndg - 1] != '0')
						++len;
				}

				wid = (wid > len) ? wid - len : 0;
				if(wid && !(flg & (PFF_JUSTIFY_LEFT | PFF_ZEROES)))
					WRITE_CHAR(buf, ' ', wid);

				if(flg & (PFF_NEGATIVE | PFF_SIGN_BLANK | PFF_SIGN_POSITIVE)) {
					if(flg & PFF_NEGATIVE)
						*buf++ = '-';
					else
						*buf++ = (flg & PFF_SIGN_BLANK) ? ' ' : '+';
				}

				if(flg & PFF_PREFIX) {
					if(rdx == 16) {
						*buf++ = '0';
						*buf++ = 'x';
					} else if(rdx == 8 && dig[ndg - 1] != '0')
						*buf++ = '0';
				}

				if(wid && (flg & PFF_ZEROES))
					WRITE_CHAR(buf, '0', wid);

				for(; (int) ndg < prc; --prc)
					*buf++ = '0';

				while(ndg)
					*buf++ = dig[--ndg];
		}

		WRITE_CHAR(buf, ' ', wid);

		++spc;
	}

	len = strlen(fmt);
	memcpy(buf, fmt, len + 1);

	return buf + len - beg;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
