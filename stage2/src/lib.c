/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

unsigned long strtoul(const char *str, char **end, int base)
{
	unsigned long value;
	unsigned dig;
	char sign;

	for(; isspace(*str); ++str)
		;

	sign = *str;
	if(sign == '+' || sign == '-')
		++str;

	if(*str == '0') {
		if(tolower(str[1]) == 'x' && (!base || base == 16)) {
			str += 2;
			base = 16;
		} else if(!base)
			base = 8;
	}
	if(!base)
		base = 10;

	for(value = 0;; ++str) {

		dig = *str;
		if(isdigit(dig))
			dig &= 0xf;
		else if(isalpha(dig))
			dig = 10 + (dig & ~0x20) - 'A';
		else
			break;

		if(dig >= base)
			break;

		if(value > (~0 - dig) / base) {
			value = ~0;
			break;
		}

		value = value * base + dig;
	}

	if(end)
		*end = (char *) str;

	return sign == '-' ? -value : value;
}

void *memmove(void *dst, const void *src, size_t size)
{
	assert(dst <= src || dst >= src + size);

	// FIXME

	return memcpy(dst, src, size);
}

char *strcpy(char *dst, const char *src)
{
	char *ptr;

	for(ptr = dst; (*dst++ = *src++);)
		;

	return dst;
}

int strncmp(const char *str1, const char *str2, size_t size)
{
	unsigned chr1, chr2;

	if(!size)
		return 0;

	do {

		chr1 = *(unsigned char *) str1;
		chr2 = *(unsigned char *) str2;
		++str1, ++str2;

	} while(--size && chr1 == chr2 && chr1);

	return (int) chr1 - (int) chr2;
}

int strcmp(const char *str1, const char *str2)
{
	for(; *str1 && *str1 == *str2; ++str1, ++str2)
		;

	return (int) *(unsigned char *) str1 - (int) *(unsigned char *) str2;
}

int strcasecmp(const char *str1, const char *str2)
{
	unsigned chr1, chr2;

	do {

		chr1 = toupper(*(unsigned char *) str1);
		chr2 = toupper(*(unsigned char *) str2);
		++str1, ++str2;

	} while(chr1 == chr2 && chr1);

	return (int) chr1 - (int) chr2;
}

int strncasecmp(const char *str1, const char *str2, size_t size)
{
	unsigned chr1, chr2;

	if(!size)
		return 0;

	do {

		chr1 = toupper(*(unsigned char *) str1);
		chr2 = toupper(*(unsigned char *) str2);
		++str1, ++str2;

	} while(--size && chr1 == chr2 && chr1);

	return (int) chr1 - (int) chr2;
}

size_t strlen(const char *str)
{
	const char *ptr;

	for(ptr = str; *ptr; ++ptr)
		;

	return ptr - str;
}

char *strchr(const char *str, int chr)
{
	for(; *str; ++str)
		if(*(unsigned char *) str == chr)
			return (char *) str;

	return NULL;
}

int sprintf(char *buf, const char *format, ...)
{
	va_list args;
	int count;

	va_start(args, format);

	count = vsprintf(buf, format, args);

	va_end(args);

	return count;
}

int printf(const char *format, ...)
{
	static char buf[256];
	va_list args;
	int count;

	va_start(args, format);

	count = vsprintf(buf, format, args);

	va_end(args);

	assert(count < sizeof(buf));

	putstring(buf);

	return count;
}

void putstring_safe(const void *str, int size)
{
	unsigned chr;

	if(size < 0)
		size = strlen(str);

	while(size--) {

		chr = *(unsigned char *) str;
		++str;

		if(isprint(chr))
			putchar(chr);
		else
			printf("\\x%02x", chr);
	}
}

int glob(const char *str, const char *pat)
{
	char chr;

	for(;; ++str) {
		chr = *pat++;
		if(*str == '\0') {
			while(chr == '*')
				chr = *pat++;
			return chr == '\0';
		}
		if(chr != '?') {
			if(chr == '*') {
				if(*pat == '\0')
					return 1;
				do
					if(glob(str, pat))
						return 1;
				while(*++str != '\0');
				return 0;
			} else if(chr != *str)
				return 0;
		}
	}
}

const char *inet_ntoa(unsigned ip)
{
	static char buf[20];

	sprintf(buf, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);

	return buf;
}

int inet_aton(const char *str, unsigned *res)
{
	unsigned ip, val, indx;
	char *ptr;

	ip = 0;

	for(indx = 1;; ++indx) {

		val = strtoul(str, &ptr, 10);
		if(ptr == str || val > 255)
			break;

		if(indx == 4) {

			if(*ptr)
				break;

			if(res)
				*res = (ip << 8) | val;

			return 1;
		}

		if(*ptr != '.')
			break;
		str = ptr + 1;

		ip = (ip << 8) | val;
	}

	return 0;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
