providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-odbc.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ODBC_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

libgda_odbc_la_SOURCES = \
	gda-odbc-provider.c \
	gda-odbc-provider.h \
	gda-odbc-recordset.c \
	gda-odbc-recordset.h \
	utils.c \
	gda-odbc.h \
	libmain.c
libgda_odbc_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_odbc_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(ODBC_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = odbc_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)

