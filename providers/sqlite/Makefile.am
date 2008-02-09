providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sqlite.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS) \
	$(sqliteinc) 


libgda_sqlite_la_SOURCES = \
	libmain.c

libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_sqlite_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(sqlitelibs) \
	$(LIBGDA_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = sqlite_specs_create_db.xml.in \
	sqlite_specs_dsn.xml.in \
	sqlite_specs_create_db.xml.in \
	sqlite_specs_drop_db.xml.in \
	sqlite_specs_create_table.xml.in \
	sqlite_specs_drop_table.xml.in \
	sqlite_specs_create_index.xml.in \
	sqlite_specs_drop_index.xml.in \
	sqlite_specs_rename_table.xml.in \
	sqlite_specs_add_column.xml.in \
	sqlite_specs_create_view.xml.in \
	sqlite_specs_drop_view.xml.in

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/virtual
gdainclude_HEADERS=$(virtualheaders)

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-sqlite-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-sqlite-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)
