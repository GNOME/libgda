if HAVE_SQLITE
sqlitelibs= $(SQLITE_LIBS)
sqliteinc= $(SQLITE_CFLAGS)
else
sqlitelibs= $(top_builddir)/providers/sqlite/sqlite-src/libsqlite.la
sqliteinc= -I$(top_srcdir)/providers/sqlite/sqlite-src \
	-I$(top_builddir)/providers/sqlite/sqlite-src 
endif

lib_LTLIBRARIES = libgda-virtual-3.0.la

noinst_PROGRAMS = virtual-test

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS) \
	-I$(top_srcdir)/providers/sqlite \
        -I$(top_builddir)/providers/sqlite \
	$(sqliteinc) 

virtual_headers = \
	gda-vconnection-data-model.h \
	gda-vconnection-hub.h \
	gda-vprovider-data-model.h \
	gda-vprovider-hub.h \
	gda-virtual-connection.h \
	gda-virtual-provider.h \
	libgda-virtual.h

libgda_virtual_3_0_la_SOURCES = \
	$(virtual_headers) \
	gda-vconnection-data-model.c \
	gda-vconnection-data-model-private.h \
	gda-vconnection-hub.c \
	gda-vprovider-data-model.c \
	gda-vprovider-hub.c \
	gda-virtual-connection.c \
	gda-virtual-provider.c \
	libmain.c

libgda_virtual_3_0_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_virtual_3_0_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/providers/sqlite/libgda-sqlite.la \
	$(sqlitelibs) \
	$(LIBGDA_LIBS)

virtual_test_SOURCES = virtual-test.c
virtual_test_DEPENDENCIES = libgda-virtual-3.0.la
virtual_test_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/providers/sqlite/virtual/libgda-virtual-3.0.la
	$(LIBGDA_LIBS)

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/virtual
gdainclude_HEADERS=$(virtual_headers)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-virtual-3.0.pc

EXTRA_DIST = $(xml_in_files) libgda-virtual-3.0.pc.in
DISTCLEANFILES = $(xml_DATA)

