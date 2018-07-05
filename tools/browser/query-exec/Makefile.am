noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/browser \
	-I$(top_srcdir)/tools/common \
	-I$(top_srcdir)/tools/base \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS)

query-marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_qe_marshal > $@
query-marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_qe_marshal > $@

GENFILES = \
	query-marshal.c \
	query-marshal.h

$(libperspective_la_OBJECTS): $(GENFILES)

libperspective_la_SOURCES = \
        $(GENFILES) \
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

$(OBJECTS): query-marshal.c query-marshal.h

EXTRA_DIST= \
	$(langspec_DATA) \
	marshal.list

CLEANFILES = \
	$(GENFILES)
