AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-diagnose gda-list-config gda-inspect-dict-file gda-author-dict-file gda-test-sql gda-test-models gda-test-connection
noinst_PROGRAMS = gda-test-sql gda-test-models

gda_diagnose_SOURCES = \
	html.h \
	html.c \
	gda-diagnose.c

gda_diagnose_LDADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
	$(LIBGDA_LIBS)

gda_inspect_dict_file_SOURCES = \
	gda-inspect-dict-file.c

gda_inspect_dict_file_LDADD = \
        $(top_builddir)/libgda/libgda-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
        $(LIBGDA_LIBS)

gda_author_dict_file_SOURCES = \
        gda-author-dict-file.c

gda_author_dict_file_LDADD = \
        $(top_builddir)/libgda/libgda-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
        $(LIBGDA_LIBS)

gda_list_config_SOURCES = \
        gda-list-config.c

gda_list_config_LDADD = \
        $(top_builddir)/libgda/libgda-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
        $(LIBGDA_LIBS)

gda_test_sql_SOURCES = \
	html.h \
	html.c \
        gda-test-sql.c

gda_test_sql_LDADD = \
        $(top_builddir)/libgda/libgda-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
        $(LIBGDA_LIBS)

gda_test_models_SOURCES = \
        html.h \
        html.c \
        gda-test-models.c

gda_test_models_LDADD = \
        $(top_builddir)/libgda/libgda-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
        $(LIBGDA_LIBS)

gda_test_connection_SOURCES = \
        gda-test-connection.c

gda_test_connection_LDADD = \
        $(top_builddir)/libgda/libgda-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
        $(LIBGDA_LIBS)

EXTRA_DIST = 
