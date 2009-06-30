noinst_LTLIBRARIES = libgda-ui-internal.la

AM_CPPFLAGS = \
	-I$(top_builddir)/ui \
	-I$(top_srcdir)/ui \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) \
	$(GTK_CFLAGS)

libgda_ui_internal_la_SOURCES = \
	gdaui-dsn-selector.c \
	gdaui-dsn-selector.h \
	gdaui-provider-auth-editor.c \
	gdaui-provider-auth-editor.h \
	gdaui-provider-spec-editor.c \
	gdaui-provider-spec-editor.h \
	utility.h \
	utility.c
