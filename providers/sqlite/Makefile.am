providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sqlite.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(SQLITE_CFLAGS) \
	$(sqliteinc) 


libgda_sqlite_la_SOURCES = \
	sqlite.gresources.c \
	libmain.c

libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_sqlite_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS)

xmldir   = $(datadir)/libgda-6.0
xml_in_files = \
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

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

# resources
RESOURCES=$(xml_in_files:.xml.in=.raw.xml)
sqlite.gresources.c: sqlite.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --generate-source $(srcdir)/sqlite.gresource.xml

%.raw.xml:%.xml.in
	@echo "  GEN      $@"
	@cat $< | sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-zA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-zA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'> $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-sqlite-6.0.pc

EXTRA_DIST = $(xml_in_files) libgda-sqlite-6.0.pc.in
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = \
	sqlite.gresources.c \
	$(RESOURCES)
