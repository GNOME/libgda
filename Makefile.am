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

dist-hook: libgda.spec
	cp libgda.spec $(distdir)

EXTRA_DIST = \
	COPYING \
	libgda.spec.in \
	libgda.pc.in \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda.pc
