bin_PROGRAMS = gda-run

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/libgda

gda_run_SOURCES = gda-run.c
gda_run_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la 

