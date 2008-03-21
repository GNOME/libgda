providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-bdb.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(BDB_CFLAGS)

libgda_bdb_la_SOURCES = \
	gda-bdb.h \
	gda-bdb-provider.c \
	gda-bdb-provider.h \
	libmain.c

libgda_bdb_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_bdb_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) 

noinst_PROGRAMS = gda-bdb-test

gda_bdb_test_SOURCES = gda-bdb-test.c
gda_bdb_test_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-4.0.la 

xmldir   = $(datadir)/libgda-4.0
xml_in_files = bdb_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-bdb-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-bdb-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)

