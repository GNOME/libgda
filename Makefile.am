SUBDIRS = \
	po \
	intl \
	macros \
	doc \
	idl \
	lib \
	bindings \
	providers \
	tools \
	testing

dist-hook: libgda.spec
	cp libgda.spec $(distdir)

EXTRA_DIST = \
	COPYING \
	libgda.spec.in \
	xml-i18n-extract.in \
	xml-i18n-merge.in \
	xml-i18n-update.in
