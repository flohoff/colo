#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= chain
OBJS=	chain.o\
		serial.o\
		start.o\

MIPSFLAGS= -mno-abicalls -fno-pic -EL -mips2 -mcpu=r5000

CFLAGS= -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-builtin -nostdlib $(MIPSFLAGS)
ASFLAGS= $(MIPSFLAGS)
CPPFLAGS= -nostdinc -Iinclude -I../include -I$(shell dirname `$(CC) --print-libgcc-file-name`)/include -D_DEBUG

all:
	$(MAKE) -C ..

binary: $(TARG).strip

$(TARG).stage2: ../stage2/stage2.strip
	echo -e '\t.section .stage2\n\t.incbin "../stage2/stage2.strip"' | $(AS) -o $@

$(TARG).strip: $(TARG)
	strip -o $@ $^

clean:
	rm -f $(TARG){,.stage2,.strip,.map,.lst} $(OBJS)

$(TARG).lst: $(TARG)
	objdump -Dx $^ > $@

$(TARG): $(OBJS) $(TARG).stage2
	$(LD) -T $(TARG).lnk -Map $(TARG).map -o $@ $^

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

%.o: src/%.S
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c -o $@ $^

# pick up serial.c from stage2 source

%.o: ../stage2/src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

.PHONY: all binary clean

# vi:set ts=3 sw=3 ai:
