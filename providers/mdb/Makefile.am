providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-mdb.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS) \
	$(MDB_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-4.0"\"

libgda_mdb_la_SOURCES = \
	gda-mdb-provider.c \
	gda-mdb-provider.h \
	gda-mdb.h \
	libmain.c 

libgda_mdb_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_mdb_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(MDB_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = mdb_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-mdb-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-mdb-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)


