#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= paneld
OBJS= paneld.o

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -mips2 -O2 -pipe -fno-strict-aliasing
CPPFLAGS= -I../lcdlib
LOADLIBES= -L../lcdlib -lpanel

$(TARG): $(OBJS)

clean:
	rm -f $(TARG) $(OBJS)

.PHONY: clean
