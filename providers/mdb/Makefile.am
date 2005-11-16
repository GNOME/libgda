providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-mdb.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(MDB_CFLAGS)

libgda_mdb_la_SOURCES = \
	gda-mdb-provider.c \
	gda-mdb-provider.h \
	gda-mdb-table.c \
	gda-mdb-table.h \
	gda-mdb.h \
	libmain.c \
	util.c
libgda_mdb_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_mdb_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS) \
	$(MDB_LIBS)

