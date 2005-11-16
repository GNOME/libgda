providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-postgres.la

configdir   = $(datadir)/libgda
config_DATA = postgres_specs_create_db.xml postgres_specs_dsn.xml


AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(POSTGRES_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_postgres_la_SOURCES = \
	gda-postgres-provider.c \
	gda-postgres-provider.h \
	gda-postgres-recordset.c \
	gda-postgres-recordset.h \
	gda-postgres-blob.c \
	gda-postgres-blob.h \
	gda-postgres.h \
	libmain.c \
	utils.c
libgda_postgres_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_postgres_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS) \
	$(POSTGRES_LIBS)

EXTRA_DIST = $(config_DATA)
