if HAVE_SQLITE
SUBDIRS=
sqlitelibs= $(SQLITE_LIBS)
sqliteinc= $(SQLITE_CFLAGS)
else
SUBDIRS= sqlite-src
sqlitelibs= $(top_builddir)/libgda/sqlite/sqlite-src/libsqlite.la
sqliteinc= -I$(top_srcdir)/libgda/sqlite/sqlite-src \
	-I$(top_builddir)/libgda/sqlite/sqlite-src -DSQLITE_API= -DSQLITE_PRIVATE=
endif
SUBDIRS+= virtual

noinst_LTLIBRARIES = libgda-sqlite.la

if HAVE_LIBCRYPTO
noinst_LTLIBRARIES += libgda-sqlcipher.la
endif

if BDBSQL
noinst_LTLIBRARIES += libgda-bdbsqlite.la
endif

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(SQLITE_CFLAGS) \
	$(sqliteinc) 

mkkeywordhash$(EXEEXT_FOR_BUILD): mkkeywordhash.c
	$(CC_FOR_BUILD) -o mkkeywordhash$(EXEEXT_FOR_BUILD) $(GDA_DEBUG_FLAGS) $<

keywords_hash.c: mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/libgda/sqlite/keywords.list
	./mkkeywordhash$(EXEEXT_FOR_BUILD) $(top_srcdir)/libgda/sqlite/keywords.list > keywords_hash.c

gen_emb_string$(EXEEXT_FOR_BUILD): gen_emb_string.c
	$(CC_FOR_BUILD) -o gen_emb_string$(EXEEXT_FOR_BUILD) $<

xml_embedded.h: gen_emb_string$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/sqlite/*.xml.in
	./gen_emb_string$(EXEEXT_FOR_BUILD) $(top_srcdir)/providers/sqlite/*.xml.in > xml_embedded.h

sqliteheaders = gda-sqlite-provider.h
sqlitesources = \
	$(sqliteheaders) \
	gda-sqlite-blob-op.c \
	gda-sqlite-blob-op.h \
	gda-sqlite-ddl.c \
	gda-sqlite-ddl.h \
	gda-sqlite-handler-bin.c \
	gda-sqlite-handler-bin.h \
	gda-sqlite-handler-boolean.c \
	gda-sqlite-handler-boolean.h \
	gda-sqlite-meta.c \
	gda-sqlite-meta.h \
	gda-sqlite-provider.c \
	gda-sqlite-pstmt.h \
	gda-sqlite-pstmt.c \
	gda-sqlite-recordset.c \
	gda-sqlite-recordset.h \
	gda-sqlite-util.c \
	gda-sqlite-util.h \
	gda-sqlite.h \
	keywords_hash.h \
	gda-symbols-util.h \
	gda-symbols-util.c

BUILT_SOURCES = xml_embedded.h keywords_hash.c

libgda_sqlite_la_SOURCES = $(sqlitesources)
libgda_sqlite_la_CFLAGS = -DPNAME=\""SQLite"\" -DCLASS_PREFIX=\""GdaSqlite"\" -DSEARCH_LIB_PATH=\""$(SQLITE_PATH)"\"
libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_sqlite_la_LIBADD = \
	virtual/libgda-virtual-5.0.la \
	$(sqlitelibs) \
	$(COREDEPS_LIBS)

if BDBSQL
libgda_bdbsqlite_la_SOURCES = $(sqlitesources)
libgda_bdbsqlite_la_CFLAGS = $(BDBSQL_CFLAGS) -DPNAME=\""BDBSql"\" -DCLASS_PREFIX=\""GdaDBDSql"\" $(BDBSQL_CFLAGS) -DWITH_BDBSQLITE -DSEARCH_LIB_PATH=\""$(BDBSQL_PATH)"\"
libgda_bdbsqlite_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_bdbsqlite_la_LIBADD = 
endif

if HAVE_LIBCRYPTO
libgda_sqlcipher_la_SOURCES = $(sqlitesources)
libgda_sqlcipher_la_CFLAGS = -DPNAME=\""SQLCipher"\" -DCLASS_PREFIX=\""GdaSQLCipher"\" -DSTATIC_SQLITE -DSQLITE_HAS_CODEC -DSEARCH_LIB_PATH=\"""\"
libgda_sqlcipher_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_sqlcipher_la_LIBADD = 
endif

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/sqlite
gdainclude_HEADERS=$(sqliteheaders)

EXTRA_DIST = gen_emb_string.c mkkeywordhash.c keywords.list

CLEANFILES = gen_emb_string$(EXEEXT_FOR_BUILD) mkkeywordhash$(EXEEXT_FOR_BUILD) $(BUILT_SOURCES)
