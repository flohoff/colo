#
# (C) P.Horton 2004,2005,2006
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

CC= $(CROSS_COMPILE)gcc
LD= $(CROSS_COMPILE)ld
AR= $(CROSS_COMPILE)ar
STRIP= $(CROSS_COMPILE)strip
OBJCOPY= $(CROSS_COMPILE)objcopy

CFLAGS_CPU:= $(shell $(CC) -mcpu=r5000 -xc -c -o /dev/null /dev/null 2> /dev/null &&\
	echo "-mcpu=r5000 -mips2 -EL" ||\
	echo "-march=r5000")

CFLAGS_COLO= -ffreestanding -mno-abicalls -fno-pic -G0

CPPFLAGS_GCC:= -I$(shell dirname `$(CC) --print-libgcc-file-name`)/include

ifneq ($(CCACHE),)
CC:= $(CCACHE) $(CC)
endif
