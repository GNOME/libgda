SUBDIRS = binreloc

bin_PROGRAMS = \
	gda-list-config-4.0 \
	gda-sql-4.0 \
	gda-list-server-op-4.0

noinst_PROGRAMS = information-schema-doc

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I. \
	$(LIBGDA_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

gda_list_config_4_0_SOURCES = \
        gda-list-config.c

gda_list_config_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_sql_4_0_SOURCES = \
        gda-sql.c \
	gda-sql.h \
	tools-input.h \
	tools-input.c \
	command-exec.h \
	command-exec.c \
	$(top_srcdir)/libgda/global.h \
	$(top_srcdir)/libgda/md5.h \
	$(top_srcdir)/libgda/md5c.c

gda_sql_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
	binreloc/libgdasql_binreloc-4.0.la \
        $(LIBGDA_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

if LIBSOUP
gda_sql_4_0_SOURCES += web-server.h web-server.c html-doc.h html-doc.c
AM_CPPFLAGS += $(LIBSOUP_CFLAGS)
gda_sql_4_0_LDADD += $(LIBSOUP_LIBS)
endif



# setenv() is in libiberty in mingw
if PLATFORM_WIN32
gda_sql_4_0_LDADD += -liberty
gda_sql_4_0_SOURCES += gda-sql-res.rc
endif

.rc.o:
	$(WINDRES) $^ -o $@

gda-sql-res.o: gda-sql-res.rc
	$(WINDRES) $^ -o $@

gda_list_server_op_4_0_SOURCES = \
        gda-list-server-op.c

gda_list_server_op_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)


information_schema_doc_SOURCES = \
        information-schema-doc.c

information_schema_doc_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

webdatadir = $(datadir)/libgda-4.0/web
webdata_DATA = \
	cnc.js \
	md5.js \
	jquery.js \
	mouseapp_2.js \
	mouseirb_2.js \
	irb.js \
	gda.css \
	gda-print.css \
	irb.css

EXTRA_DIST = gda-sql.ico \
	$(webdata_DATA)
