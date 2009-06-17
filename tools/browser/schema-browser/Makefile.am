noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS)

libperspective_la_SOURCES = \
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
	mgr-columns.c \
	mgr-columns.h
