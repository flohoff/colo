#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARG= stage2
TOOLDIR= ../tools
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
		net.o\
		arp.o\
		ip.o\
		icmp.o\
		udp.o\
		start.o\

LOADADDR=0x80000000

CC_DEP:= $(shell $(CC) -mcpu=r5000 -xc -c -o /dev/null /dev/null 2> /dev/null &&\
	echo "-mcpu=r5000 -EL" ||\
	echo "-mtune=r5000 -G0")

MIPSFLAGS= -mno-abicalls -fno-pic -mips2 $(CC_DEP)

LDFLAGS= -T $(TARG).lnk -Map $(TARG).map --emit-relocs --section-start .text=$(LOADADDR)
CFLAGS= -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-builtin -nostdlib -fno-strict-aliasing $(MIPSFLAGS)
ASFLAGS= $(MIPSFLAGS)
CPPFLAGS= -nostdinc -Iinclude -I../include -I$(shell dirname `$(CC) --print-libgcc-file-name`)/include -D_DEBUG

all:
	$(MAKE) -C ..

binary: $(TARG).elf

$(TARG).bin: $(TARG)
	objcopy -O binary $^ $@

$(TARG).elf: $(TARG).rfx
	echo -e '\t.section .stage2\n\t.incbin "$^"' | $(AS) -o $@

$(TARG).rfx: $(TARG)
	$(TOOLDIR)/elf2rfx/elf2rfx -f $^ $@

$(TARG).tmp: $(TARG)
	$(TOOLDIR)/elf2rfx/elf2rfx -fb $(LOADADDR) $^ $@

test-rfx: $(TARG).bin $(TARG).tmp
	cmp $^
	@echo '***  SUCCESS  ***'

clean:
	rm -f $(TARG){,.elf,.rfx,.map,.bin,.tmp} $(OBJS)

$(TARG): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

%.o: src/%.S
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c -o $@ $^

.PHONY: all binary clean test-rfx

# vi:set ts=3 sw=3 ai:
