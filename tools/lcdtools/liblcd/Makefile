#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= liblcd.a
OBJS= panel.o

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -mips2 -O2 -pipe -fno-strict-aliasing

$(TARG): $(OBJS)
	$(AR) r $@ $^

clean:
	rm -f $(TARG) $(OBJS)

.PHONY: clean
