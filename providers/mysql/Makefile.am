providerdir=$(libdir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-mysql.la

#Rem: MYSQL_CFLAGS and MYSQL_LIBS are the compile and link flags necessary to use the
# C API. It is specific to the API and should be computed in the configure.in script.

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) $(MYSQL_CFLAGS) 

mkkeywordhash$(EXEEXT_FOR_BUILD): $(top_srcdir)/libgda/sqlite/mkkeywordhash.c
	$(CC_FOR_BUILD) -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keyword_files=keywords_V50.list keywords_V51.list keywords_V54.list keywords_V60.list
pkeyword_files=$(addprefix $(top_srcdir)/providers/mysql/,$(keyword_files))

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(pkeyword_files)
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/mysql/keywords_V50.list V50 > keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/mysql/keywords_V51.list V51 >> keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/mysql/keywords_V54.list V54 >> keywords_hash.c
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/mysql/keywords_V60.list V60 >> keywords_hash.c

# parser generation
parser.c parser.h: parser.y $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD)
	- $(top_builddir)/libgda/sql-parser/lemon$(EXEEXT_FOR_BUILD) -q -d $(srcdir)/parser.y $(top_srcdir)/libgda/sql-parser/lempar.c

gen_def$(EXEEXT_FOR_BUILD): gen_def.c
	$(CC_FOR_BUILD) -o gen_def$(EXEEXT_FOR_BUILD) -DIMPOSED_HEADER=\""$(top_srcdir)/libgda/sql-parser/token_types.h"\" $(srcdir)/gen_def.c

mysql_token_types.h: gen_def$(EXEEXT_FOR_BUILD) parser.h
	./gen_def$(EXEEXT_FOR_BUILD) > mysql_token_types.h

$(OBJECTS) $(libgda_mysql_la_OBJECTS): mysql_token_types.h keywords_hash.c

libgda_mysql_la_SOURCES = \
	gda-mysql-blob-op.c \
	gda-mysql-blob-op.h \
	gda-mysql-ddl.c \
	gda-mysql-ddl.h \
	gda-mysql-parser.c \
	gda-mysql-parser.h \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql-meta.c \
	gda-mysql-meta.h \
	gda-mysql-recordset.c \
	gda-mysql-recordset.h \
	gda-mysql-util.c \
	gda-mysql-util.h \
	gda-mysql-pstmt.c \
	gda-mysql-pstmt.h \
	gda-mysql.h \
	libmain.c \
	parser.h \
        parser.c \
        mysql_token_types.h

libgda_mysql_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED) $(LIBTOOL_PROV_EXPORT_OPTIONS)
libgda_mysql_la_LIBADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS) $(MYSQL_LIBS)

xmldir   = $(datadir)/libgda-4.0
xml_in_files = \
	mysql_specs_create_db.xml.in \
	mysql_specs_drop_db.xml.in \
	mysql_specs_dsn.xml.in \
	mysql_specs_create_table.xml.in \
        mysql_specs_drop_table.xml.in \
        mysql_specs_create_index.xml.in \
        mysql_specs_drop_index.xml.in \
	mysql_specs_rename_table.xml.in \
	mysql_specs_add_column.xml.in \
	mysql_specs_drop_column.xml.in \
	mysql_specs_create_view.xml.in \
	mysql_specs_drop_view.xml.in

@INTLTOOL_XML_RULE@

xml_DATA = $(xml_in_files:.xml.in=.xml) 

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgda-mysql-4.0.pc

EXTRA_DIST = $(xml_in_files) libgda-mysql-4.0.pc.in parser.y gen_def.c $(keyword_files)
DISTCLEANFILES = $(xml_DATA)

CLEANFILES = parser.h parser.c parser.out mysql_token_types.h gen_def$(EXEEXT_FOR_BUILD) \
	mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c
