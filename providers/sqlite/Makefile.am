SUBDIRS = sqlite-src

providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sqlite.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-I$(top_srcdir)/providers/sqlite/sqlite-src \
	-I$(top_builddir)/providers/sqlite/sqlite-src \
	$(SQLITE_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_sqlite_la_SOURCES = \
	gda-sqlite-ddl.c \
	gda-sqlite-ddl.h \
	gda-sqlite-handler-bin.c \
	gda-sqlite-handler-bin.h \
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

xmldir   = $(datadir)/libgda
xml_in_files = sqlite_specs_create_db.xml.in \
	sqlite_specs_dsn.xml.in \
	sqlite_specs_create_db.xml.in \
	sqlite_specs_drop_db.xml.in \
	sqlite_specs_create_table.xml.in \
	sqlite_specs_drop_table.xml.in \
	sqlite_specs_create_index.xml.in \
	sqlite_specs_drop_index.xml.in \
	sqlite_specs_rename_table.xml.in \
	sqlite_specs_add_column.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)
