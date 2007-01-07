providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-firebird.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(FIREBIRD_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

libgda_firebird_la_SOURCES = \
	gda-firebird-provider.c \
	gda-firebird-provider.h \
	gda-firebird-recordset.c \
	gda-firebird-recordset.h \
	libmain.c

libgda_firebird_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_firebird_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(FIREBIRD_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = firebird_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)



