#
# (C) P.Horton 2004
#
# $Id$
#

TARGET= boot.bin
STAGE1= stage1/stage1.bin
STAGE2= stage2/stage2.strip
SUBDIRS= stage1 stage2 chain

all: subdirs $(TARGET)

ci: clean
	EDITOR=vi svn ci

$(TARGET): $(STAGE1) $(STAGE2)
	rm -f $@
	cat $^ > $@

subdirs:
	for x in $(SUBDIRS); do $(MAKE) -C $$x binary; done

clean:
	rm -f $(TARGET)
	for x in $(SUBDIRS); do $(MAKE) -C $$x clean; done

.PHONY: ci all subdirs clean
