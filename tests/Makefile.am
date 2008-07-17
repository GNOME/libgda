noinst_LTLIBRARIES = libgda-test-4.0.la
noinst_PROGRAMS = test-ddl-creator

SUBDIRS = providers parser value-holders meta-store data-models multi-threading

AM_CPPFLAGS = \
        -I$(top_builddir) \
        -I$(top_srcdir) \
        -I$(top_srcdir)/libgda \
        $(LIBGDA_CFLAGS)

test_headers = \
        gda-ddl-creator.h

libgda_test_4_0_la_SOURCES = \
        $(test_headers) \
        gda-ddl-creator.c

libgda_test_4_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgda_test_4_0_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-4.0.la 

test_ddl_creator_SOURCES = \
        test-ddl-creator.c

test_ddl_creator_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
	libgda-test-4.0.la \
        $(LIBGDA_LIBS)
