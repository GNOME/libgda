noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
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
	data-console.h \
	data-console.c \
	data-source.h \
	data-source.c \
	data-widget.h \
	data-widget.c \
	data-favorite-selector.c \
	data-favorite-selector.h \
	perspective-main.c \
	perspective-main.h \
	xml-spec-editor.c \
	xml-spec-editor.h \
	ui-spec-editor.c \
	ui-spec-editor.h \
	data-manager-perspective.h \
	data-manager-perspective.c \
	data-source-editor.h \
	data-source-editor.c \
	data-source-manager.h \
	data-source-manager.c

$(OBJECTS): marshal.c marshal.h

EXTRA_DIST= \
        marshal.list

CLEANFILES = \
        marshal.h \
        marshal.c