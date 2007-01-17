providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sybase.la

AM_CPPFLAGS = \
	$(SYBASE_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DLIBGDA_DATA_DIR=\""$(datadir)/libgda-3.0"\"

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

libgda_sybase_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_sybase_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS) \
	$(SYBASE_LIBS)

xmldir   = $(datadir)/libgda-3.0
xml_in_files = sybase_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml)

EXTRA_DIST = $(xml_in_files)
DISTCLEANFILES = $(xml_DATA)


