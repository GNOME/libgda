noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) -DHAVE_LDAP \
	$(MAC_INTEGRATION_CFLAGS)

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_ldap_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_ldap_marshal > $@

libperspective_la_SOURCES = \
	marshal.c \
	marshal.h \
	perspective-main.h \
	perspective-main.c \
	ldap-browser-perspective.h \
	ldap-browser-perspective.c \
	ldap-favorite-selector.h \
	ldap-favorite-selector.c \
	ldap-entries-page.c \
	ldap-entries-page.h \
	mgr-ldap-entries.h \
	mgr-ldap-entries.c \
	hierarchy-view.h \
	hierarchy-view.c \
	entry-properties.h \
	entry-properties.c \
	mgr-ldap-classes.h \
	mgr-ldap-classes.c \
	classes-view.h \
	classes-view.c \
	ldap-classes-page.h \
	ldap-classes-page.c \
	class-properties.h \
	class-properties.c \
	filter-editor.h \
	filter-editor.c \
	ldap-search-page.h \
	ldap-search-page.c \
	vtable-dialog.h \
	vtable-dialog.c

$(OBJECTS): marshal.c marshal.h

EXTRA_DIST = \
	marshal.list

CLEANFILES = \
	marshal.h \
	marshal.c
