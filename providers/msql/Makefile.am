providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-msql.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(MSQL_CFLAGS)

libgda_msql_la_SOURCES = \
	gda-msql-provider.c \
	gda-msql-provider.h \
	gda-msql-recordset.c \
	gda-msql-recordset.h \
	gda-msql.h \
	libmain.c \
	gda-msql-utils.c
libgda_msql_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_msql_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(MSQL_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = msql_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-msql-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-msql-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)


