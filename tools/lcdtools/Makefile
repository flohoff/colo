#
# (C) P.Horton 2004
#
# $Id$
#
# This code is covered by the GNU General Public License. For details see the file "COPYING".
#

SUBDIRS= liblcd putlcd paneld e2fsck-lcd

SUBDIRS+= $(if $(wildcard /usr/lib/liblua50.*), fpanel)

all:
	set -e; for x in $(SUBDIRS); do $(MAKE) -C $$x; done

clean:
	for x in $(SUBDIRS); do $(MAKE) -C $$x clean; done

.PHONY: all clean
