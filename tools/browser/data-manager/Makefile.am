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
	$(GLIB_GENMARSHAL) $< --header --prefix=_dm_marshal > $@
marshal.c: marshal.list $(GLIB_GENMARSHAL) marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_dm_marshal > $@

libperspective_la_SOURCES = \
        marshal.c \
        marshal.h \
	analyser.c \
	analyser.h \
	data-console.c \
	data-console.h \
	data-favorite-selector.c \
	data-favorite-selector.h \
	data-manager-perspective.c \
	data-manager-perspective.h \
	data-source.c \
	data-source-editor.c \
	data-source-editor.h \
	data-source.h \
	data-source-manager.c \
	data-source-manager.h \
	data-widget.c \
	data-widget.h \
	perspective-main.c \
	perspective-main.h \
	ui-spec-editor.c \
	ui-spec-editor.h \
	xml-spec-editor.c \
	xml-spec-editor.h


$(OBJECTS): marshal.c marshal.h

EXTRA_DIST= \
	$(langspec_DATA) \
	marshal.list

CLEANFILES = \
	marshal.h \
	marshal.c
