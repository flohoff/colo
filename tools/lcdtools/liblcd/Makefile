#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= liblcd.a
OBJS= liblcd.o lcddev.o lcdraw.o

include ../../../Rules.mak

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -pipe -fno-strict-aliasing $(CFLAGS_CPU)
CPPFLAGS= -DBUILDING_LIBLCD

$(TARG): $(OBJS)
	$(AR) r $@ $^

clean:
	rm -f $(TARG) $(OBJS)

.PHONY: clean
