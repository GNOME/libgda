bin_PROGRAMS = ddl

ddl_SOURCES = ddl.c

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/libgda \
	$(COREDEPS_CFLAGS)

AM_LDFLAGS = $(top_builddir)/libgda/libgda-6.0.la \
        $(COREDEPS_LIBS)
