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

void *memcpy(void *dst, const void *src, size_t size)
{
	void *ptr, *end;

	if(!size)
		return dst;

	ptr = dst;
	end = ptr + size;

	while(ptr < end && ((unsigned long) ptr & 3)) {
		*(uint8_t *) ptr = *(uint8_t *) src;
		++ptr, ++src;
	}

	if(!((unsigned long) src & 3))
		while(ptr < end - 3) {
			*(uint32_t *) ptr = *(uint32_t *) src;
			ptr += 4, src += 4;
		}

	while(ptr < end) {
		*(uint8_t *) ptr = *(uint8_t *) src;
		++ptr, ++src;
	}

	return dst;
}

void *memmove(void *dst, const void *src, size_t size)
{
	assert(dst <= src || dst >= src + size);

	// FIXME

	return memcpy(dst, src, size);
}

void *memset(void *dst, int val, size_t size)
{
	void *ptr, *end;

	if(!size)
		return dst;

	val &= 0xff;
	val |= val << 8;
	val |= val << 16;

	ptr = dst;
	end = ptr + size;

	while(ptr < end && ((unsigned long) ptr & 3)) {
		*(uint8_t *) ptr = val;
		++ptr;
	}

	while(ptr < end - 3) {
		*(uint32_t *) ptr = val;
		ptr += 4;
	}

	while(ptr < end) {
		*(uint8_t *) ptr = val;
		++ptr;
	}

	return dst;
}

int memcmp(const void *mem1, const void *mem2, size_t size)
{
	unsigned dat1, dat2;

	if(!size)
		return 0;

	do {

		dat1 = *(unsigned char *) mem1;
		dat2 = *(unsigned char *) mem2;
		++mem1, ++mem2;

	} while(--size && dat1 == dat2);

	return (int) dat1 - (int) dat2;
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

/* vi:set ts=3 sw=3 cin path=include,../include: */
