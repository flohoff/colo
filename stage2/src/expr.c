/*
 * (C) P.Horton 2004,2005,2006
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#include "lib.h"

unsigned long evaluate(const char *str, char **end)
{
	unsigned long value;
	char *ptr;

	value = strtoul(str, &ptr, 10);

	if(*ptr == 't' || *ptr == 'T')
		++ptr;
	else
		value = strtoul(str, &ptr, 16);

	if(end)
		*end = ptr;

	return value;
}

/* vi:set ts=3 sw=3 cin path=include,../include: */
