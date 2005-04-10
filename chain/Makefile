#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= colo-chain
OBJS=	chain.o\
		libmem.o\
		start.o\
		lcd.o\

XOBJS= ../stage2/stage2.elf\

include ../Rules.mak

DEPS=$(OBJS:.o=.d)

LDFLAGS= -nostdlib -Wl,-T,$(TARG).lnk,-Map,$(TARG).map
CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-strict-aliasing $(CFLAGS_CPU) $(CFLAGS_COLO)
CPPFLAGS= -MD -nostdinc -Iinclude -I../include -D_DEBUG $(CPPFLAGS_GCC)

all:
	$(MAKE) -C .. binary

binary: $(TARG).elf

$(TARG).elf: $(TARG)
	$(STRIP) -o $@ $<

clean:
	rm -f $(TARG){,.elf,.map} $(OBJS) $(XOBJS) $(DEPS)

$(TARG): $(OBJS) $(XOBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o: src/%.S
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

libmem.o: ../stage2/src/libmem.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

lcd.o: ../stage1/src/lcd.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)

.PHONY: all binary clean

# vi:set ts=3 sw=3 ai:
