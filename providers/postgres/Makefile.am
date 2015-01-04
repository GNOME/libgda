providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-postgres.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	-I$(top_srcdir)/providers/reuseable/postgres \
	$(COREDEPS_CFLAGS) $(COREDEPS_WFLAGS) $(POSTGRES_CFLAGS) 

libgda_postgres_la_SOURCES = \
	gda-postgres-blob-op.c \
	gda-postgres-blob-op.h \
	gda-postgres-ddl.c \
	gda-postgres-ddl.h \
	gda-postgres-handler-bin.c \
	gda-postgres-handler-bin.h \
	gda-postgres-provider.c \
	gda-postgres-provider.h \
	gda-postgres-pstmt.c \
	gda-postgres-pstmt.h \
	gda-postgres-recordset.c \
	gda-postgres-recordset.h \
	gda-postgres-util.c \
	gda-postgres-util.h \
	gda-postgres.h \
	postgres.gresources.c \
	libmain.c

libgda_postgres_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_postgres_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/providers/reuseable/postgres/libgda-postgres.la \
	$(COREDEPS_LIBS) $(POSTGRES_LIBS)

xmldir   = $(datadir)/libgda-6.0
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
        postgres_specs_drop_view.xml.in \
	postgres_specs_create_user.xml.in \
	postgres_specs_create_role.xml.in \
	postgres_specs_drop_user.xml.in \
	postgres_specs_drop_role.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

# resources
RESOURCES=$(xml_in_files:.xml.in=.raw.xml)
postgres.gresources.c: postgres.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --sourcedir=$(builddir) --generate-source $(srcdir)/postgres.gresource.xml

%.raw.xml:%.xml.in
	@echo "  GEN      $@"
	@cat $< | sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-zA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-zA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'> $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-postgres-6.0.pc

CLEANFILES = \
	postgres.gresources.c \
	$(RESOURCES)

EXTRA_DIST = $(xml_in_files) \
	libgda-postgres-6.0.pc.in \
	postgres.gresource.xml
DISTCLEANFILES = $(xml_DATA)
