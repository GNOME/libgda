SUBDIRS = \
	po \
	idl \
	libsql \
	libgda \
	providers \
	report \
	tools \
	testing \
	data \
	doc

EXTRA_DIST = \
	COPYING \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda.pc

all-local: libgda.spec
