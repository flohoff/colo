#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

TARGET1= colo-rom-image.bin
TARGET2= colo-chain.elf
STAGE1= stage1/$(TARGET1)
CHAIN= chain/$(TARGET2)
SUBDIRS= tools/elf2rfx stage2 stage1 chain
TOOLDIRS= tools/flash-tool tools/putlcd tools/e2fsck-lcd
BINDIR= binaries

all: binary tooldirs

binary: subdirs $(TARGET1) $(TARGET2)

ci:
	rm -f $(BINDIR)/$(TARGET1) $(BINDIR)/$(TARGET2)
	EDITOR=vi svn ci

$(TARGET1): $(STAGE1)
	cp -f $^ $@
	cp -f $^ $(BINDIR)

$(TARGET2): $(CHAIN)
	cp -f $^ $@
	cp -f $^ $(BINDIR)

subdirs:
	set -e; for x in $(SUBDIRS); do $(MAKE) -C $$x binary; done

tooldirs:
	set -e; for x in $(TOOLDIRS); do $(MAKE) -C $$x; done

clean:
	rm -f $(TARGET1) $(TARGET2)
	for x in $(SUBDIRS) $(TOOLDIRS); do $(MAKE) -C $$x clean; done

.PHONY: all binary ci subdirs tooldirs clean
