providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-xbase.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(XBASE_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

libgda_xbase_la_SOURCES = \
	gda-xbase-database.cpp \
	gda-xbase-database.h \
	gda-xbase-provider.c \
	gda-xbase-provider.h \
	libmain.c

libgda_xbase_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_xbase_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(XBASE_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = xbase_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)


