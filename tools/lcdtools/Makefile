#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

CFLAGS= -Wall -Wstrict-prototypes -fomit-frame-pointer -mips2 -O2 -pipe -fno-strict-aliasing

all: putlcd paneld

putlcd: putlcd.o panel.o

paneld: paneld.o panel.o

clean:
	rm -f putlcd putlcd.o paneld paneld.o panel.o

.PHONY: all clean
