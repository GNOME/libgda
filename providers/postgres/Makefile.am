providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-postgres.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(POSTGRES_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_postgres_la_SOURCES = \
	gda-postgres-ddl.c \
	gda-postgres-ddl.h \
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

xmldir   = $(datadir)/libgda
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

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)
