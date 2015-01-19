providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-bdb.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(BDB_CFLAGS)

libgda_bdb_la_SOURCES = \
	gda-bdb.h \
	gda-bdb-provider.c \
	gda-bdb-provider.h \
	bdb.gresources.c \
	libmain.c

libgda_bdb_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_bdb_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) 

noinst_PROGRAMS = gda-bdb-test

gda_bdb_test_SOURCES = gda-bdb-test.c
gda_bdb_test_LDADD = \
	$(COREDEPS_LIBS) \
	$(BDB_LIBS) \
	$(top_builddir)/libgda/libgda-6.0.la 

xmldir   = $(datadir)/libgda-6.0
xml_in_files = bdb_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

# resources
RESOURCES=$(xml_in_files:.xml.in=.raw.xml)
bdb.gresources.c: bdb.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/bdb.gresource.xml

%.raw.xml:%.xml.in
	@echo "  GEN      $@"
	@cat $< | sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-ZA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-ZA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'> $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-bdb-6.0.pc

EXTRA_DIST = $(xml_in_files) libgda-bdb-6.0.pc.in
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = \
	bdb.gresources.c \
	$(RESOURCES)
