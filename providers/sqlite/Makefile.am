providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-sqlite.la

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS)

libgda_sqlite_la_SOURCES = \
	gda-sqlite-provider.c \
	gda-sqlite-provider.h \
	gda-sqlite-recordset.c \
	gda-sqlite-recordset.h \
	gda-sqlite.h \
	libmain.c
libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_sqlite_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(SQLITE_LIBS)

