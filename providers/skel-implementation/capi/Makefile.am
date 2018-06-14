providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
#provider_LTLIBRARIES = libgda-capi.la
noinst_LTLIBRARIES = libgda-capi.la

#Rem: CAPI_CFLAGS and CAPI_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) $(COREDEPS_WFLAGS) #$(CAPI_CFLAGS) 

# SQL keywords HASH generation
keyword_files=keywords.list
pkeyword_files=$(addprefix $(top_srcdir)/providers/skel-implementation/capi/,$(keyword_files))

mkkeywordhash$(EXEEXT_FOR_BUILD): $(top_srcdir)/libgda/sqlite/mkkeywordhash.c
	$(CC_FOR_BUILD) -g -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(pkeyword_files)
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/skel-implementation/capi/keywords.list > keywords_hash.c

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d -o$(builddir) -T$(top_srcdir)/libgda/sql-parser/lempar.c $(srcdir)/parser.y

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_builddir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

capi_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > capi_token_types.h

$(OBJECTS) $(libgda_capi_la_OBJECTS): capi_token_types.h keywords_hash.c

libgda_capi_la_SOURCES = \
	gda-capi-blob-op.c \
	gda-capi-blob-op.h \
	gda-capi-ddl.c \
	gda-capi-ddl.h \
	gda-capi-parser.c \
	gda-capi-parser.h \
	gda-capi-provider.c \
	gda-capi-provider.h \
	gda-capi-pstmt.h \
	gda-capi-pstmt.c \
	gda-capi-meta.c \
	gda-capi-meta.h \
	gda-capi-recordset.c \
	gda-capi-recordset.h \
	gda-capi.h \
	capi.gresources.c \
	libmain.c \
	parser.h \
        parser.c \
        capi_token_types.h

libgda_capi_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_capi_la_LIBADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS) #$(CAPI_LIBS)

xmldir   = $(datadir)/libgda-6.0
xml_in_files = \
	capi_specs_dsn.xml.in \
	capi_specs_create_table.xml.in 

@INTLTOOL_XML_RULE@

#xml_DATA = $(xml_in_files:.xml.in=.xml) 
noinst_DATA = $(xml_in_files:.xml.in=.xml)

# resources
RESOURCES=$(xml_in_files:.xml.in=.raw.xml)
capi.gresources.c: capi.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --generate-source $(srcdir)/capi.gresource.xml

%.raw.xml:%.xml.in
	@echo "  GEN      $@"
	@cat $< | sed -e 's/_\([a-zA-Z0-9]*\)=/\1=/g' -e 's/<_\([a-zA-Z0-9_]*\)>/<\1>/g' -e 's/<\/_\([a-zA-Z0-9_]*\)>/<\/\1>/g' -e 's/<!-- .* -->//'> $@

pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = libgda-capi-6.0.pc
noinst_DATA += libgda-capi-6.0.pc

EXTRA_DIST = $(xml_in_files) libgda-capi-6.0.pc.in parser.y gen_def.c $(keyword_files)
DISTCLEANFILES = $(noinst_DATA)

CLEANFILES = parser.h parser.c parser.out capi_token_types.h gen_def$(EXEEXT_FOR_BUILD) \
	mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c \
	capi.gresources.c \
	$(RESOURCES)
