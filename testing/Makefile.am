INCLUDES = \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test 

noinst_PROGRAMS = \
	gda-xmldb-test

gda_test_SOURCES = gda-test.c
gda_test_LDADD = \
	$(top_builddir)/libgda/libgda-2.la \
	$(LIBGDA_LIBS)

gda_xmldb_test_SOURCES = gda-xmldb-test.c
gda_xmldb_test_LDADD = \
	$(top_builddir)/libgda/libgda-2.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = sample_database.xml
