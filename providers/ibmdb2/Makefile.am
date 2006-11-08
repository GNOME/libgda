providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-ibmdb2.la

AM_CPPFLAGS = \
	$(IBMDB2_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_ibmdb2_la_SOURCES = \
	gda-ibmdb2-provider.c \
	gda-ibmdb2-provider.h \
	gda-ibmdb2-recordset.c \
	gda-ibmdb2-recordset.h \
	gda-ibmdb2-types.c \
	gda-ibmdb2-types.h \
	gda-ibmdb2.h \
	libmain.c \
	utils.c

libgda_ibmdb2_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_ibmdb2_la_LIBADD = \
	$(IBMDB2_LIBS) \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.0.la 


xmldir   = $(datadir)/libgda
xml_in_files = ibmdb2_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)


