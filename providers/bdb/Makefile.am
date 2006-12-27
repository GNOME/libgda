providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-bdb.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(BDB_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

libgda_bdb_la_SOURCES = \
	gda-bdb-provider.c \
	gda-bdb-provider.h \
	gda-bdb-recordset.c \
	gda-bdb-recordset.h \
	gda-bdb.h \
	utils.c \
	libmain.c
libgda_bdb_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_bdb_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(BDB_LIBS)

#bin_PROGRAMS = gda-bdb-test
#gda_bdb_test_SOURCES = gda-bdb-test.c
#gda_bdb_test_LDADD = \
#	$(LIBGDA_LIBS) \
#	$(top_builddir)/libgda/libgda-3.0.la \
#	$(BDB_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = bdb_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)

