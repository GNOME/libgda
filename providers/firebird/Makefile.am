providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES =

if FIREBIRD_CLIENT
provider_LTLIBRARIES+=libgda-firebird-client.la
endif

if FIREBIRD_EMBED
provider_LTLIBRARIES+=libgda-firebird-embed.la
endif

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) $(COREDEPS_WFLAGS) $(FIREBIRD_CFLAGS)

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d -T$(top_srcdir)/libgda/sql-parser/lempar.c $(srcdir)/parser.y

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_srcdir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

firebird_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > firebird_token_types.h

$(OBJECTS) $(libgda_firebird_la_OBJECTS): firebird_token_types.h

firebird_sources = \
	gda-firebird-provider.c \
	gda-firebird-provider.h \
	gda-firebird-blob-op.c \
	gda-firebird-blob-op.h \
	gda-firebird-ddl.c \
	gda-firebird-ddl.h \
	gda-firebird-parser.c \
	gda-firebird-parser.h \
	gda-firebird-pstmt.h \
	gda-firebird-pstmt.c \
	gda-firebird-meta.c \
	gda-firebird-meta.h \
	gda-firebird-recordset.c \
	gda-firebird-recordset.h \
	gda-firebird-util.c \
	gda-firebird-util.h \
	gda-firebird.h \
	parser.h \
	parser.c \
	firebird_token_types.h

libgda_firebird_client_la_SOURCES = $(firebird_sources) libmain-client.c
libgda_firebird_client_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_firebird_client_la_LIBADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS) $(FIREBIRD_CLIENT_LIBS)

libgda_firebird_embed_la_SOURCES = $(firebird_sources) libmain-embed.c
libgda_firebird_embed_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_firebird_embed_la_LIBADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS) $(FIREBIRD_EMBED_LIBS)

xmldir   = $(datadir)/libgda-5.0
xml_in_files = \
	firebird_specs_dsn.xml.in \
	firebird_specs_create_table.xml.in 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-firebird-5.0.pc

EXTRA_DIST = $(xml_in_files) libgda-firebird-5.0.pc.in parser.y gen_def.c
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = parser.h parser.c parser.out firebird_token_types.h gen_def$(EXEEXT_FOR_BUILD)
