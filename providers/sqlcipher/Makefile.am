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
	$(COREDEPS_CFLAGS) \
	$(LIBCRYPTO_CFLAGS) \
	$(sqliteinc) \
	-DSQLITE_HAS_CODEC -DSQLITE_API= -DSQLITE_PRIVATE= -DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_THREADSAFE=1


sqlite_sources = sqlite3.c sqlite3.h

libmain.lo: sqlite3.h

libgda_sqlcipher_la_SOURCES = \
	gda-sqlcipher-provider.c \
	sqlcipher.gresources.c \
	libmain.c

libgda_sqlcipher_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_sqlcipher_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) \
	$(LIBCRYPTO_LIBS)

xmldir   = $(datadir)/libgda-6.0
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

# resources
RESOURCES=$(xml_in_files:.xml.in=.raw.xml)
sqlcipher.gresources.c: sqlcipher.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --generate-source $(srcdir)/sqlcipher.gresource.xml

%.raw.xml:%.xml.in
	@echo "  GEN      $@"
	@cat $< | sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-zA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-zA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'> $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-sqlcipher-6.0.pc

EXTRA_DIST = $(xml_in_files) \
	libgda-sqlcipher-6.0.pc.in \
	COPYING.sqlcipher \
	sqlcipher.gresource.xml

CLEANFILES = \
	sqlcipher.gresources.c \
	$(RESOURCES)

DISTCLEANFILES = $(xml_DATA)
