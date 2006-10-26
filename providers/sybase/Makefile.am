providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sybase.la

AM_CPPFLAGS = \
	$(SYBASE_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda"\"

libgda_sybase_la_SOURCES = \
	gda-sybase-provider.c \
	gda-sybase-provider.h \
	gda-sybase-recordset.c \
	gda-sybase-recordset.h \
	gda-sybase-types.c \
	gda-sybase-types.h \
	gda-sybase-schemas.h \
	gda-sybase.h \
	libmain.c \
	utils.c

libgda_sybase_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_sybase_la_LIBADD = \
	$(SYBASE_LIBS) \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la 

xmldir   = $(datadir)/libgda
xml_in_files = sybase_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)


