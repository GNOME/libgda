providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-mdb.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(MDB_CFLAGS)

libgda_mdb_la_SOURCES = \
	gda-mdb-provider.c \
	gda-mdb-provider.h \
	gda-mdb.h \
	libmain.c
libgda_mdb_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_mdb_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(MDB_LIBS)

