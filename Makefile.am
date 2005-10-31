DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

SUBDIRS = \
	libsql \
	libgda \
	providers \
	report \
	tools \
	testing \
	data \
	po \
	doc

EXTRA_DIST = \
	COPYING \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	mkinstalldirs

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda.pc

all-local: libgda.spec
