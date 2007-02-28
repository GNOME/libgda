DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

SUBDIRS = \
	po \
	win32 \
	libsql \
	libgda \
	providers \
	report \
	tools \
	gda-sharp \
	tests \
	testing \
	data \
	doc

EXTRA_DIST = \
	COPYING \
	gda-sharp.snk \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	mkinstalldirs

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda-3.0.pc

all-local: libgda.spec

CLEANFILES = intltool-extract intltool-merge intltool-update

