providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-postgres.la

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	$(POSTGRES_CFLAGS)

libgda_postgres_la_SOURCES = \
	gda-postgres-provider.c \
	gda-postgres-provider.h \
	gda-postgres-recordset.c \
	gda-postgres-recordset.h \
	gda-postgres.h \
	libmain.c \
	utils.c
libgda_postgres_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_postgres_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(POSTGRES_LIBS)

