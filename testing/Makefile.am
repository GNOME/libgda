## -------------------------------------------------------------------------
## Makefile.am for gda test programs
## -------------------------------------------------------------------------

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS)

## -------------------------------------------------------------------------

bin_PROGRAMS = gda-test

gda_test_SOURCES = \
	gda-test.c

gda_test_LDADD = \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-client/libgda-client.la \
	$(GDA_CLIENT_LIBS)
