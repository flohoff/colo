#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= elf2rfx
OBJS= elf2rfx.o

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -pipe -fno-strict-aliasing

binary: $(TARG)

$(TARG): $(OBJS)

clean:
	rm -rf $(TARG) $(OBJS)

.PHONY: binary clean
