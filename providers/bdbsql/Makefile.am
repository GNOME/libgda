providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-bdbsql.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(BDBSQL_CFLAGS) \
	$(sqliteinc) 


libgda_bdbsql_la_SOURCES = \
	libmain.c

libgda_bdbsql_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_bdbsql_la_LIBADD = \
	$(top_builddir)/libgda/sqlite/libgda-bdbsqlite.la

xmldir   = $(datadir)/libgda-4.0
xml_in_files = \
	bdbsql_specs_dsn.xml.in \
	bdbsql_specs_create_db.xml.in \
	bdbsql_specs_drop_db.xml.in \
	bdbsql_specs_create_table.xml.in \
	bdbsql_specs_drop_table.xml.in \
	bdbsql_specs_create_index.xml.in \
	bdbsql_specs_drop_index.xml.in \
	bdbsql_specs_rename_table.xml.in \
	bdbsql_specs_add_column.xml.in \
	bdbsql_specs_create_view.xml.in \
	bdbsql_specs_drop_view.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-bdbsql-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-bdbsql-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)
