providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-oracle.la

#Rem: ORACLE_CFLAGS and ORACLE_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS) 

keyword_files=keywords_8.list keywords_9.list
pkeyword_files=$(addprefix $(top_srcdir)/providers/oracle/,$(keyword_files))
mkkeywordhash$(EXEEXT_FOR_BUILD): $(top_srcdir)/libgda/sqlite/mkkeywordhash.c
	$(CC_FOR_BUILD) -g -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(pkeyword_files)
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/oracle/keywords_8.list V8 > keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/oracle/keywords_9.list V9 >> keywords_hash.c

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d $(srcdir)/parser.y $(top_srcdir)/libgda/sql-parser/lempar.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_builddir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

oracle_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > oracle_token_types.h

$(OBJECTS) $(libgda_oracle_la_OBJECTS): oracle_token_types.h keywords_hash.c

libgda_oracle_la_SOURCES = \
	gda-oracle-blob-op.c \
	gda-oracle-blob-op.h \
	gda-oracle-ddl.c \
	gda-oracle-ddl.h \
	gda-oracle-parser.c \
	gda-oracle-parser.h \
	gda-oracle-provider.c \
	gda-oracle-provider.h \
	gda-oracle-pstmt.h \
	gda-oracle-pstmt.c \
	gda-oracle-meta.c \
	gda-oracle-meta.h \
	gda-oracle-recordset.c \
	gda-oracle-recordset.h \
	gda-oracle-util.c \
	gda-oracle-util.h \
	gda-oracle.h \
	libmain.c \
	parser.h \
        parser.c \
        oracle_token_types.h

libgda_oracle_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_oracle_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) $(ORACLE_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = \
	oracle_specs_dsn.xml.in \
	oracle_specs_create_table.xml.in 

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-oracle-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-oracle-4.0.pc.in parser.y gen_def.c $(keyword_files)
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = parser.h parser.c parser.out oracle_token_types.h gen_def$(EXEEXT_FOR_BUILD) \
	mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c
