providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
#provider_LTLIBRARIES = libgda-models.la
noinst_LTLIBRARIES = libgda-models.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) 

libgda_models_la_SOURCES = \
	gda-models-provider.c \
	gda-models-provider.h \
	gda-models.h \
	libmain.c

libgda_models_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_models_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) 

xmldir   = $(datadir)/libgda-4.0
xml_in_files = models_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

#xml_DATA = $(xml_in_files:.xml.in=.xml)
noinst_DATA = $(xml_in_files:.xml.in=.xml)

pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = libgda-models-4.0.pc
noinst_DATA += libgda-models-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-models-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)

