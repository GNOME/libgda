SUBDIRS = \
	po \
	idl \
	lib \
	bindings \
	providers \
	tools \
	testing \
	doc

dist-hook: libgda.spec
	cp libgda.spec $(distdir)

EXTRA_DIST = \
	COPYING \
	libgda.spec.in \
	libgda-client.pc.in \
	libgda-server.pc.in \
	xml-i18n-extract.in \
	xml-i18n-merge.in \
	xml-i18n-update.in

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda-client.pc libgda-server.pc