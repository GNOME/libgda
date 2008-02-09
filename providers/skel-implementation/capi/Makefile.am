providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-capi.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(CAPI_CFLAGS) 

libgda_capi_la_SOURCES = \
	gda-capi-blob-op.c \
	gda-capi-blob-op.h \
	gda-capi-cursor-recordset.c \
	gda-capi-cursor-recordset.h \
	gda-capi-ddl.c \
	gda-capi-ddl.h \
	gda-capi-handler-bin.c \
	gda-capi-handler-bin.h \
	gda-capi-parser.c \
	gda-capi-parser.h \
	gda-capi-provider.c \
	gda-capi-provider.h \
	gda-capi-recordset.c \
	gda-capi-recordset.h \
	gda-capi.h \
	libmain.c \
	parser.h \
	parser.c \
	capi_token_types.h \
	utils.c
libgda_capi_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_capi_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) \
	$(CAPI_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = \
	capi_specs_create_db.xml.in \
	capi_specs_drop_db.xml.in \
	capi_specs_dsn.xml.in \
	capi_specs_create_table.xml.in \
	capi_specs_drop_table.xml.in \
	capi_specs_create_index.xml.in \
	capi_specs_drop_index.xml.in \
	capi_specs_rename_table.xml.in \
	capi_specs_add_column.xml.in \
	capi_specs_drop_column.xml.in \
	capi_specs_create_view.xml.in \
	capi_specs_drop_view.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-capi-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-capi-4.0.pc.in
DISTCLEANFILES = $(xml_DATA)
