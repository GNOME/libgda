noinst_LTLIBRARIES = libperspective.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/tools \
	-I$(top_srcdir)/tools/base \
	-I$(top_srcdir)/tools/common \
	-I$(top_srcdir)/tools/browser \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	$(MAC_INTEGRATION_CFLAGS)

$(libperspective_la_OBJECTS): $(GENFILES)

libperspective_la_SOURCES = \
	$(GENFILES) \
	perspective-main.c \
	perspective-main.h \
	dummy-perspective.h \
	dummy-perspective.c

CLEANFILES = \
	$(GENFILES)
