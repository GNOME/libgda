providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-bdb.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(BDB_CFLAGS)

libgda_bdb_la_SOURCES = \
	gda-bdb-provider.c \
	gda-bdb-provider.h \
	gda-bdb-recordset.c \
	gda-bdb-recordset.h \
	gda-bdb.h \
	utils.c \
	libmain.c
libgda_bdb_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_bdb_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(BDB_LIBS)

#bin_PROGRAMS = gda-bdb-test
#gda_bdb_test_SOURCES = gda-bdb-test.c
#gda_bdb_test_LDADD = \
#	$(LIBGDA_LIBS) \
#	$(top_builddir)/libgda/libgda-2.la \
#	$(BDB_LIBS)
