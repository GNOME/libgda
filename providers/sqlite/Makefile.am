SUBDIRS = sqlite-src

providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sqlite.la

configdir   = $(datadir)/libgda
config_DATA = sqlite_specs_create_db.xml sqlite_specs_dsn.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-I$(top_srcdir)/providers/sqlite/sqlite-src \
	-I$(top_builddir)/providers/sqlite/sqlite-src \
	$(SQLITE_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_sqlite_la_SOURCES = \
	gda-sqlite-provider.c \
	gda-sqlite-provider.h \
	gda-sqlite-recordset.c \
	gda-sqlite-recordset.h \
	gda-sqlite.h \
	utils.c \
	libmain.c

libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_sqlite_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(top_builddir)/providers/sqlite/sqlite-src/libsqlite.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = $(config_DATA)
