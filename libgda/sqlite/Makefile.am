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


sqliteheaders = gda-sqlite-provider.h
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
	utils.c 

libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version $(NO_UNDEFINED)
libgda_sqlite_la_LIBADD = \
	virtual/libgda-virtual-4.0.la \
	$(sqlitelibs) \
	$(LIBGDA_LIBS)

gdaincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda/sqlite
gdainclude_HEADERS=$(sqliteheaders)

