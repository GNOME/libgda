DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

if BUILD_GDA_SHARP
GDA_SHARP_DIRECTORY=gda-sharp
endif

SUBDIRS = \
	po \
	libsql \
	libgda \
	providers \
	report \
	tools \
	$(GDA_SHARP_DIRECTORY) \
	testing \
	data \
	doc

EXTRA_DIST = \
	COPYING \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	mkinstalldirs

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda-2.0.pc

all-local: libgda.spec

CLEANFILES = intltool-extract intltool-merge intltool-update

