#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= flash-tool
OBJS= flash-tool.o

include ../../Rules.mak

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -pipe -fno-strict-aliasing $(CFLAGS_CPU)

$(TARG): $(OBJS)

clean:
	rm -f $(TARG) $(OBJS)

.PHONY: clean
