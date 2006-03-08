providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-oracle.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_oracle_la_SOURCES = \
	gda-oracle-provider.c \
	gda-oracle-provider.h \
	gda-oracle-recordset.c \
	gda-oracle-recordset.h \
	gda-oracle.h \
	libmain.c \
	utils.c
libgda_oracle_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_oracle_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la \
	$(ORACLE_LIBS)

xmldir   = $(datadir)/libgda
xml_in_files = oracle_specs_dsn.xml.in
xml_DATA = $(xml_in_files:.xml.in=.xml)

@INTLTOOL_XML_RULE@

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)
