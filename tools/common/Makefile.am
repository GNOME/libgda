noinst_LTLIBRARIES = libcommon.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/base \
        $(COREDEPS_CFLAGS) \
        $(COREDEPS_WFLAGS)

common-marshal.h: marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=_marshal > $@
common-marshal.c: marshal.list $(GLIB_GENMARSHAL) common-marshal.h
	$(GLIB_GENMARSHAL) $< --body --prefix=_marshal > $@

nonuisources = \
	common-marshal.h \
	common-marshal.c \
	t-decl.h \
	t-app.h \
	t-app.c \
	t-context.h \
	t-context.c \
	t-term-context.h \
	t-term-context.c \
	t-web-context.h \
	t-web-context.c \
	t-errors.h \
	t-errors.c \
	t-favorites.h \
	t-favorites.c \
	t-utils.h \
	t-utils.c \
	t-config-info.h \
	t-config-info.c \
	t-connection.h \
	t-connection.c \
	t-virtual-connection.h \
	t-virtual-connection.c \
	t-favorites-actions.h \
	t-favorites-actions.c

if LIBSOUP
nonuisources += web-server.h web-server.c html-doc.h html-doc.c
AM_CPPFLAGS += $(LIBSOUP_CFLAGS)
endif

libcommon_la_SOURCES = $(nonuisources)

EXTRA_DIST = \
	marshal.list

if HAVE_UI
noinst_LTLIBRARIES += libcommonui.la

libcommonui_la_CFLAGS = \
	$(AM_CPPFLAGS) \
        $(GTK_CFLAGS)

libcommonui_la_SOURCES = \
	$(nonuisources)

endif
