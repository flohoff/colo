#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

STAGE2= ../../stage2

# pickup local lib.h

CPPFLAGS= -I. -I$(STAGE2)/include
CFLAGS= -Wall -Wstrict-prototypes -fomit-frame-pointer -mips2 -O2 -pipe -fno-strict-aliasing

md5rom: md5rom.o md5.o

%.o: $(STAGE2)/src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf md5rom md5rom.o md5.o

.PHONY: clean
