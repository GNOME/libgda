providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-capi.la

#Rem: CAPI_CFLAGS and CAPI_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) #$(CAPI_CFLAGS) 

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) parser.y

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_builddir)/libgda/sql-parser/parser_tokens.h"\" gen_def.c

capi_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > capi_token_types.h

$(OBJECTS) $(libgda_capi_la_OBJECTS): capi_token_types.h

libgda_capi_la_SOURCES = \
	gda-capi-blob-op.c \
	gda-capi-blob-op.h \
	gda-capi-ddl.c \
	gda-capi-ddl.h \
	gda-capi-parser.c \
	gda-capi-parser.h \
	gda-capi-provider.c \
	gda-capi-provider.h \
	gda-capi-meta.c \
	gda-capi-meta.h \
	gda-capi-recordset.c \
	gda-capi-recordset.h \
	gda-capi.h \
	libmain.c \
	parser.h \
        parser.c \
        capi_token_types.h

libgda_capi_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_capi_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) #$(CAPI_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = \
	capi_specs_dsn.xml.in \
	capi_specs_create_table.xml.in 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-capi-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-capi-4.0.pc.in parser.y
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = parser.h parser.c parser.out capi_token_types.h gen_def$(EXEEXT_FOR_BUILD)
