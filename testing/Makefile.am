## -------------------------------------------------------------------------
## Makefile.am for gda test programs
## -------------------------------------------------------------------------

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS)

## -------------------------------------------------------------------------

bin_PROGRAMS = gda-test 

noinst_PROGRAMS = gda-primebase-test gda-sybase-test gda-tds-test

gda_test_SOURCES = \
	gda-test.c

gda_primebase_test_SOURCES = \
	gda-primebase-test.c

gda_sybase_test_SOURCES = \
	gda-sybase-test.c

gda_tds_test_SOURCES = \
	gda-tds-test.c

gda_test_LDADD = \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-client/libgda-client-2.la \
	$(GDA_CLIENT_LIBS)

gda_primebase_test_LDADD = $(gda_test_LDADD)
gda_sybase_test_LDADD = $(gda_test_LDADD)
gda_tds_test_LDADD = $(gda_test_LDADD)

EXTRA_DIST = sample_database.xml
