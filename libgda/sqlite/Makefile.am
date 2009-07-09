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

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
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

$(libgda_sqlite_la_OBJECTS): xml_embedded.h keywords_hash.c

libgda_sqlite_la_SOURCES = \
	$(sqliteheaders) \
	gda-sqlite-blob-op.c \
	gda-sqlite-blob-op.h \
	gda-sqlite-ddl.c \
	gda-sqlite-ddl.h \
	gda-sqlite-handler-bin.c \
	gda-sqlite-handler-bin.h \
	gda-sqlite-meta.c \
	gda-sqlite-meta.h \
	gda-sqlite-provider.c \
	gda-sqlite-pstmt.h \
	gda-sqlite-pstmt.c \
	gda-sqlite-recordset.c \
	gda-sqlite-recordset.h \
	gda-sqlite.h \
	keywords_hash.h \
	utils.c

libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_sqlite_la_LIBADD = \
	virtual/libgda-virtual-4.0.la \
	$(sqlitelibs) \
	$(LIBGDA_LIBS)

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/sqlite
gdainclude_HEADERS=$(sqliteheaders)

EXTRA_DIST = gen_emb_string.c mkkeywordhash.c keywords.list

CLEANFILES = gen_emb_string$(EXEEXT_FOR_BUILD) xml_embedded.h mkkeywordhash$(EXEEXT_FOR_BUILD) keywords_hash.c
