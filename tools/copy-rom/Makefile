#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= copy-rom
OBJS= copy-rom.o md5.o
STAGE2= ../../stage2

include ../../Rules.mak

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -pipe -fno-strict-aliasing $(CFLAGS_CPU)
CPPFLAGS= -I. -I$(STAGE2)/include

$(TARG): $(OBJS)

%.o: $(STAGE2)/src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf $(TARG) $(OBJS)

dist: $(TARG)
	rm -rf $(OBJS)

.PHONY: clean
