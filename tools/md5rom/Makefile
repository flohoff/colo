#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= md5rom
OBJS= md5rom.o md5.o
STAGE2= ../../stage2

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -mips2 -O2 -pipe -fno-strict-aliasing
CPPFLAGS= -I$(STAGE2)/include

$(TARG): $(OBJS)

%.o: $(STAGE2)/src/%.c
	$(CC) -I. $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf $(TARG) $(OBJS)

dist: $(TARG)
	rm -rf $(OBJS)

.PHONY: clean
