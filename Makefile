#
# (C) P.Horton 2004
#
# $Id$
#

TARGET1= boot.bin
TARGET2= chain.bin
STAGE1= stage1/stage1.bin
STAGE2= stage2/stage2.strip
CHAIN= chain/chain.strip
SUBDIRS= stage1 stage2 chain

all: subdirs $(TARGET1) $(TARGET2)

ci: clean
	EDITOR=vi svn ci

$(TARGET1): $(STAGE1) $(STAGE2)
	rm -f $@
	cat $^ > $@

$(TARGET2): $(CHAIN)
	cp -f $^ $@

subdirs:
	for x in $(SUBDIRS); do $(MAKE) -C $$x binary; done

clean:
	rm -f $(TARGET1) $(TARGET2)
	for x in $(SUBDIRS); do $(MAKE) -C $$x clean; done

.PHONY: ci all subdirs clean
