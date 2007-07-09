if HAVE_SQLITE
sqlitelibs= $(SQLITE_LIBS)
sqliteinc= $(SQLITE_CFLAGS)
else
sqlitelibs= $(top_builddir)/providers/sqlite/sqlite-src/libsqlite.la
sqliteinc= -I$(top_srcdir)/providers/sqlite/sqlite-src \
	-I$(top_builddir)/providers/sqlite/sqlite-src
endif

providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-vproviders.la

noinst_PROGRAMS = vproviders-test

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS) \
	$(sqliteinc) 

libgda_vproviders_la_SOURCES = \
	gda-vconnection-data-model.c \
	gda-vconnection-data-model.h \
	gda-vprovider-data-model.c \
	gda-vprovider-data-model.h \
	gda-virtual-connection.c \
	gda-virtual-connection.h \
	gda-virtual-provider.c \
	gda-virtual-provider.h

libgda_vproviders_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_vproviders_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/providers/sqlite/libgda-sqlite.la \
	$(sqlitelibs) \
	$(LIBGDA_LIBS)

vproviders_test_SOURCES = vproviders-test.c
vproviders_test_DEPENDENCIES = libgda-vproviders.la
vproviders_test_LDADD = \
	$(top_builddir)/providers/sqlite/vproviders/libgda-vproviders.la
	$(LIBGDA_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-vproviders-3.0.pc

EXTRA_DIST = $(xml_in_files) libgda-vproviders-3.0.pc.in
DISTCLEANFILES = $(xml_DATA)
