#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= e2fsck-lcd
OBJS= e2fsck-lcd.o

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -pipe -mips2 -fno-strict-aliasing
CPPFLAGS= -I../liblcd
LOADLIBES= -L../liblcd -llcd

$(TARG): $(OBJS)

clean:
	rm -f $(TARG) $(OBJS)

.PHONY: clean
