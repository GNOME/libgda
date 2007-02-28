providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-postgres.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(POSTGRES_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

libgda_postgres_la_SOURCES = \
	gda-postgres-blob-op.c \
	gda-postgres-blob-op.h \
	gda-postgres-ddl.c \
	gda-postgres-ddl.h \
	gda-postgres-handler-bin.c \
	gda-postgres-handler-bin.h \
	gda-postgres-provider.c \
	gda-postgres-provider.h \
	gda-postgres-recordset.c \
	gda-postgres-recordset.h \
	gda-postgres.h \
	libmain.c \
	utils.c
libgda_postgres_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_postgres_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
	$(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
	$(LIBGDA_LIBS) \
	$(POSTGRES_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = \
	postgres_specs_create_db.xml.in \
	postgres_specs_drop_db.xml.in \
	postgres_specs_dsn.xml.in \
	postgres_specs_create_table.xml.in \
	postgres_specs_drop_table.xml.in \
	postgres_specs_create_index.xml.in \
	postgres_specs_drop_index.xml.in \
	postgres_specs_rename_table.xml.in \
	postgres_specs_add_column.xml.in \
	postgres_specs_drop_column.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-postgres-3.0.pc

EXTRA_DIST = $(xml_in_files) libgda-postgres-3.0.pc.in
DISTCLEANFILES = $(xml_DATA)
