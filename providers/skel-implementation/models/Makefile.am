providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
#provider_LTLIBRARIES = libgda-models.la
noinst_LTLIBRARIES = libgda-models.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_CFLAGS)

libgda_models_la_SOURCES = \
	gda-models-provider.c \
	gda-models-provider.h \
	gda-models.h \
	models.gresources.c \
	libmain.c

libgda_models_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_models_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) 

xmldir   = $(datadir)/libgda-6.0
xml_in_files = models_specs_dsn.xml.in

@INTLTOOL_XML_RULE@

#xml_DATA = $(xml_in_files:.xml.in=.xml)
noinst_DATA = $(xml_in_files:.xml.in=.xml)

# resources
RESOURCES=$(xml_in_files:.xml.in=.raw.xml)
models.gresources.c: models.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --generate-source $(srcdir)/models.gresource.xml

%.raw.xml:%.xml.in
	@echo "  GEN      $@"
	@cat $< | sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-zA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-zA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'> $@

pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = libgda-models-6.0.pc
noinst_DATA += libgda-models-6.0.pc

EXTRA_DIST = $(xml_in_files) libgda-models-6.0.pc.in models.gresource.xml
DISTCLEANFILES = $(noinst_DATA)

CLEANFILES = \
        models.gresources.c \
        $(RESOURCES)
