#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= fpanel
OBJS= fpanel.o

include ../../../Rules.mak

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -pipe -fno-strict-aliasing $(CFLAGS_CPU)
CPPFLAGS= -I../liblcd
LOADLIBES= -L../liblcd -llcd -llua50 -llualib50 -lreadline

$(TARG): $(OBJS)

clean:
	rm -f $(TARG) $(OBJS)

.PHONY: clean
