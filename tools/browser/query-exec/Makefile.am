noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS)

libperspective_la_SOURCES = \
	query-console.c \
	query-console.h \
	query-editor.c \
	query-editor.h \
	perspective-main.c \
	perspective-main.h \
	query-exec-perspective.h \
	query-exec-perspective.c

langspecdir=$(datadir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/language-specs
langspec_DATA= \
	gda-sql.lang