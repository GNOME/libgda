noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS)

libperspective_la_SOURCES = \
	perspective-main.c \
	perspective-main.h \
	dummy-perspective.h \
	dummy-perspective.c
