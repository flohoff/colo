#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= stage2
OBJS=	main.o\
		lib.o\
		libmem.o\
		vsprintf.o\
		serial.o\
		shell.o\
		expr.o\
		edit.o\
		history.o\
		memory.o\
		srec.o\
		ide.o\
		block.o\
		ext2.o\
		md5.o\
		pci.o\
		cache.o\
		elf.o\
		inflate.o\
		tulip.o\
		keymap.o\
		flash.o\
		heap.o\
		launch.o\
		start.o\

CC_DEP:= $(shell $(CC) -mcpu=r5000 -xc -c -o /dev/null /dev/null 2> /dev/null &&\
	echo "-mcpu=r5000 -EL" ||\
	echo "-mtune=r5000 -G0")

MIPSFLAGS= -mno-abicalls -fno-pic -mips2 $(CC_DEP)

CFLAGS= -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-builtin -nostdlib $(MIPSFLAGS)
ASFLAGS= $(MIPSFLAGS)
CPPFLAGS= -nostdinc -Iinclude -I../include -I$(shell dirname `$(CC) --print-libgcc-file-name`)/include -D_DEBUG

all:
	$(MAKE) -C ..

binary: $(TARG).strip

$(TARG).strip: $(TARG)
	strip -o $@ $^

clean:
	rm -f $(TARG){,.strip,.map,.lst} $(OBJS)

dist: $(TARG)
	rm -f $(TARG){.map,.lst} $(OBJS)

$(TARG).lst: $(TARG)
	objdump -Dx $^ > $@

$(TARG): $(OBJS)
	$(LD) -T $(TARG).lnk -Map $(TARG).map -o $@ $^

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

%.o: src/%.S
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c -o $@ $^

.PHONY: all binary clean

# vi:set ts=3 sw=3 ai:
