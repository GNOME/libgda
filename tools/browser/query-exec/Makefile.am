noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS)

marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_qe_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_qe_marshal > $@

libperspective_la_SOURCES = \
        marshal.c \
        marshal.h \
	query-console-page.c \
	query-console-page.h \
	query-editor.c \
	query-editor.h \
	query-favorite-selector.c \
	query-favorite-selector.h \
	query-result.c \
	query-result.h \
	perspective-main.c \
	perspective-main.h \
	query-exec-perspective.h \
	query-exec-perspective.c

$(OBJECTS): marshal.c marshal.h

EXTRA_DIST= \
	$(langspec_DATA) \
	marshal.list

CLEANFILES = \
	marshal.h \
	marshal.c
