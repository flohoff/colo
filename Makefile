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
BINDIR= binaries

all: subdirs $(TARGET1) $(TARGET2)

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

clean:
	rm -f $(TARGET1) $(TARGET2)
	for x in $(SUBDIRS); do $(MAKE) -C $$x clean; done

.PHONY: ci all subdirs clean
