SUBDIRS = \
	po \
	intl \
	macros \
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
	xml-i18n-extract.in \
	xml-i18n-merge.in \
	xml-i18n-update.in
