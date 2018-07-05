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

data-manager-marshal.h: data-manager-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_dm_marshal > $@
data-manager-marshal.c: data-manager-marshal.list $(GLIB_GENMARSHAL) data-manager-marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_dm_marshal > $@

GENFILES = \
	data-manager-marshal.c \
	data-manager-marshal.h

$(libperspective_la_OBJECTS): $(GENFILES)

libperspective_la_SOURCES = \
        $(GENFILES) \
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

EXTRA_DIST= \
	$(langspec_DATA) \
	data-manager-marshal.list

CLEANFILES = \
	$(GENFILES)
