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
		../stage2/stage2.elf\

CC_DEP:= $(shell $(CC) -mcpu=r5000 -xc -c -o /dev/null /dev/null 2> /dev/null &&\
	echo "-mcpu=r5000 -EL" ||\
	echo "-mtune=r5000 -G0")

CC_INC:= $(shell dirname `$(CC) --print-libgcc-file-name`)/include

MIPSFLAGS= -mno-abicalls -fno-pic -mips2 $(CC_DEP)

CFLAGS= -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -O2 -fno-builtin -nostdlib -fno-strict-aliasing $(MIPSFLAGS)
ASFLAGS= $(MIPSFLAGS)
CPPFLAGS= -nostdinc -Iinclude -I../include -I$(CC_INC) -D_DEBUG

all:
	$(MAKE) -C ..

binary: $(TARG).bin

clean:
	rm -f $(TARG){,.bin,.map} $(OBJS)

$(TARG).bin: $(TARG)
	objcopy -O binary $^ $@

$(TARG): $(OBJS)
	$(LD) -T $@.lnk -Map $@.map -o $@ $^

fast.o: CFLAGS+= -Wa,--no-warn

%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $^

%.o: src/%.S
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c -o $@ $^

.PHONY: all binary clean

# vi:set ts=3 sw=3 ai:
