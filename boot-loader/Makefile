#
# (C) P.Horton 2004
#
# $Id$
#

BACKUP= 192.168.1.189

TARGET= boot.bin
STAGE1= stage1/stage1.bin
STAGE2= stage2/stage2
STRIP= stage2.strip
SUBDIRS= stage1 stage2

all: subdirs $(TARGET)

$(TARGET): $(STAGE1) $(STRIP)
	rm -f $@
	cat $^ > $@

$(STRIP): $(STAGE2)
	strip -o $@ $^

subdirs:
	for x in $(SUBDIRS); do $(MAKE) -C $$x binary; done

clean:
	rm -f $(TARGET) $(STRIP)
	for x in $(SUBDIRS); do $(MAKE) -C $$x clean; done

backup: clean
	id=$$(basename $$(pwd)) && rm -f ../$$id.tar.gz && tar zcvf ../$$id.tar.gz -C .. $$id && scp ../$$id.tar.gz $(BACKUP):

.PHONY: all subdirs clean backup
