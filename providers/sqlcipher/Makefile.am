providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sqlcipher.la

# We don't use $(COREDEPS_WFLAGS) because we use a copy of the sqlite sources,
# which we don't want to fix ourselves.
# TODO: Report sqlite warnings upstream so we can use warnings for the rest 
# of our source code here too. 
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(LIBCRYPTO_CFLAGS) \
	$(sqliteinc) \
	-DSQLITE_HAS_CODEC -DSQLITE_API= -DSQLITE_PRIVATE= -DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1


sqlite_sources = sqlite3.c sqlite3.h
sqlite3.c: $(srcdir)/sqlcipher.patch $(top_srcdir)/libgda/sqlite/sqlite-src/sqlite3.c
	cp $(top_srcdir)/libgda/sqlite/sqlite-src/sqlite3.c sqlite3.c
	patch -p0 < $(srcdir)/sqlcipher.patch

sqlite3.h: $(top_srcdir)/libgda/sqlite/sqlite-src/sqlite3.h
	cp $(top_srcdir)/libgda/sqlite/sqlite-src/sqlite3.h sqlite3.h

libmain.lo: sqlite3.h

libgda_sqlcipher_la_SOURCES = \
	$(sqlite_sources) \
	libmain.c

libgda_sqlcipher_la_LDFLAGS = -export-dynamic -module -avoid-version $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_sqlcipher_la_LIBADD = \
	$(top_builddir)/libgda/sqlite/libgda-sqlcipher.la \
	$(LIBCRYPTO_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = \
	sqlcipher_specs_auth.xml.in \
	sqlcipher_specs_dsn.xml.in \
	sqlcipher_specs_create_db.xml.in \
	sqlcipher_specs_drop_db.xml.in \
	sqlcipher_specs_create_table.xml.in \
	sqlcipher_specs_drop_table.xml.in \
	sqlcipher_specs_create_index.xml.in \
	sqlcipher_specs_drop_index.xml.in \
	sqlcipher_specs_rename_table.xml.in \
	sqlcipher_specs_add_column.xml.in \
	sqlcipher_specs_create_view.xml.in \
	sqlcipher_specs_drop_view.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-sqlcipher-4.0.pc

EXTRA_DIST = $(xml_in_files) \
	libgda-sqlcipher-4.0.pc.in \
	sqlcipher.patch \
	COPYING.sqlcipher

CLEANFILES = $(sqlite_sources)

DISTCLEANFILES = $(xml_DATA)
