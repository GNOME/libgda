providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-postgres.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) $(POSTGRES_CFLAGS) 

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d $(srcdir)/parser.y $(top_srcdir)/libgda/sql-parser/lempar.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_srcdir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

postgres_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > postgres_token_types.h

$(OBJECTS) $(libgda_postgres_la_OBJECTS): postgres_token_types.h

libgda_postgres_la_SOURCES = \
	gda-postgres-blob-op.c \
	gda-postgres-blob-op.h \
	gda-postgres-ddl.c \
	gda-postgres-ddl.h \
	gda-postgres-handler-bin.c \
	gda-postgres-handler-bin.h \
	gda-postgres-parser.c \
	gda-postgres-parser.h \
	gda-postgres-provider.c \
	gda-postgres-provider.h \
	gda-postgres-pstmt.c \
	gda-postgres-pstmt.h \
	gda-postgres-meta.c \
	gda-postgres-meta.h \
	gda-postgres-recordset.c \
	gda-postgres-recordset.h \
	gda-postgres-util.c \
	gda-postgres-util.h \
	gda-postgres.h \
	libmain.c \
	parser.h \
        parser.c \
        postgres_token_types.h

libgda_postgres_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_postgres_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) $(POSTGRES_LIBS)

xmldir   = $(datadir)/libgda-4.0
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
        postgres_specs_drop_column.xml.in \
        postgres_specs_create_view.xml.in \
        postgres_specs_drop_view.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-postgres-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-postgres-4.0.pc.in parser.y gen_def.c
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = parser.h parser.c parser.out postgres_token_types.h gen_def$(EXEEXT_FOR_BUILD)
