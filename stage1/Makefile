#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= colo-rom-image
OBJS=	init.o\
		main.o\
		lcd.o\
		dram.o\
		loader.o\
		fast.o\
		start.o\

XOBJS= ../stage2/stage2.elf\

include ../Rules.mak

DEPS=$(OBJS:.o=.d)

LDFLAGS= -nostdlib -Wl,-T,$(TARG).lnk,-Map,$(TARG).map
CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-strict-aliasing $(CFLAGS_CPU) $(CFLAGS_COLO)
CPPFLAGS= -MD -nostdinc -Iinclude -I../include -D_DEBUG $(CPPFLAGS_GCC)

all:
	$(MAKE) -C .. binary

binary: $(TARG).bin

clean:
	rm -f $(TARG){,.bin,.map} $(OBJS) $(XOBJS) $(DEPS)

$(TARG).bin: $(TARG)
	$(OBJCOPY) -O binary $< $@

$(TARG): $(OBJS) $(XOBJS)
	$(CC) $(LDFLAGS) -o $@ $^

fast.o: CFLAGS+= -Wa,--no-warn

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o: src/%.S
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

-include $(DEPS)

.PHONY: all binary clean

# vi:set ts=3 sw=3 ai:
