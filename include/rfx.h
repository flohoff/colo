/*
 * (C) P.Horton 2004
 *
 * $Id$
 *
 * This code is covered by the GNU General Public License. For details see the file "COPYING".
 */

#ifndef _RFX_H_
#define _RFX_H_

#define RFX_REL_32			0
#define RFX_REL_26			1
#define RFX_REL_H16			2
/* #define RFX_REL_L16		3 */

#define RFX_HDR_MAGIC		"\xaaRFX"
#define RFX_HDR_MAGIC_SZ	4

struct rfx_header
{
	char		magic[RFX_HDR_MAGIC_SZ];
	unsigned	imgsize;
	unsigned	memsize;
	unsigned	entry;
	unsigned	nrelocs;
};

#endif

/* vi:set ts=3 sw=3 cin: */
