#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= colo-chain
OBJS=	chain.o\
		serial.o\
		libmem.o\
		start.o\
		../stage2/stage2.elf\

CC_DEP:= $(shell $(CC) -mcpu=r5000 -xc -c -o /dev/null /dev/null 2> /dev/null &&\
	echo "-mcpu=r5000 -EL" ||\
	echo "-mtune=r5000 -G0")

MIPSFLAGS= -mno-abicalls -fno-pic -mips2 $(CC_DEP)

CFLAGS= -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-builtin -nostdlib $(MIPSFLAGS)
ASFLAGS= $(MIPSFLAGS)
CPPFLAGS= -nostdinc -Iinclude -I../include -I$(shell dirname `$(CC) --print-libgcc-file-name`)/include -D_DEBUG

all:
	$(MAKE) -C ..

binary: $(TARG).elf

$(TARG).elf: $(TARG).unstripped
	strip -o $@ $^

clean:
	rm -f $(TARG){,.elf,.map,.lst,.unstripped} $(OBJS)

$(TARG).lst: $(TARG)
	objdump -Dx $^ > $@

$(TARG).unstripped: $(OBJS)
	$(LD) -T $(TARG).lnk -Map $(TARG).map -o $@ $^

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

%.o: src/%.S
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c -o $@ $^

# pick up libmem.c & serial.c from stage2 source

%.o: ../stage2/src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

.PHONY: all binary clean

# vi:set ts=3 sw=3 ai:
