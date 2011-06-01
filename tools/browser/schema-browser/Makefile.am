noinst_LTLIBRARIES = libperspective.la

if LDAP
ldap_flags=-DHAVE_LDAP
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(ldap_flags) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS)

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_sb_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_sb_marshal > $@

libperspective_la_SOURCES = \
	marshal.c \
	marshal.h \
	perspective-main.h \
	perspective-main.c \
	schema-browser-perspective.h \
	schema-browser-perspective.c \
	favorite-selector.c \
	favorite-selector.h \
	objects-index.c \
	objects-index.h \
	table-info.c \
	table-info.h \
	table-columns.c \
	table-columns.h \
	table-preferences.c \
	table-preferences.h \
	mgr-columns.c \
	mgr-columns.h

$(OBJECTS): marshal.c marshal.h

if HAVE_GOOCANVAS
libperspective_la_SOURCES += \
	table-relations.c \
	table-relations.h \
	relations-diagram.c \
	relations-diagram.h
endif

EXTRA_DIST = \
	marshal.list

CLEANFILES = \
	marshal.h \
	marshal.c
