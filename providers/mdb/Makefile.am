providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-mdb.la

if MDBEMBEDDED
SUBDIRS = libmdb-src
extra_mdb_cflags = -I$(srcdir)/libmdb-src/include
extra_mdb_ldflags = libmdb-src/libmdb.la
if CROSS_COMPILING
else
if PLATFORM_WIN32
extra_mdb_ldflags = -liconv libmdb-src/libmdb.la
endif
endif
else
extra_mdb_cflags = $(MDB_CFLAGS)
extra_mdb_ldflags = $(MDB_LIBS)
endif

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(extra_mdb_cflags) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-5.0"\"

libgda_mdb_la_SOURCES = \
	gda-mdb-provider.c \
	gda-mdb-provider.h \
	gda-mdb.h \
	libmain.c 

libgda_mdb_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_mdb_la_LIBADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS) \
	$(extra_mdb_ldflags)

xmldir   = $(datadir)/libgda-5.0
xml_in_files = mdb_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-mdb-5.0.pc

EXTRA_DIST = $(xml_in_files) libgda-mdb-5.0.pc.in
DISTCLEANFILES = $(xml_DATA)


