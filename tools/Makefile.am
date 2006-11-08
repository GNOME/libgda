if BUILD_CONFIG_TOOL
CONFIG_TOOL=gda-config-tool
man_MANS = gda-config-tool.1 \
           gda-config.5
else
CONFIG_TOOL=
endif

if BUILD_RUN_TOOL
RUN_TOOL=gda-run
else
RUN_TOOL=
endif

bin_PROGRAMS = $(RUN_TOOL) $(CONFIG_TOOL) gda-diagnose gda-list-config gda-inspect-dict-file gda-author-dict-file

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I. \
	$(LIBGDA_CFLAGS)

gda_run_SOURCES = gda-run.c
gda_run_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/libgda/graph/libgda_graph-3.0.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
	$(LIBGDA_LIBS)

gda_config_tool_SOURCES = gda-config-tool.c
gda_config_tool_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/libgda/graph/libgda_graph-3.0.la \
	$(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
	$(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
	$(READLINE_LIB) \
	$(HISTORY_LIB) \
	$(POPT_LIB) \
	$(LIBGDA_LIBS)

gda_diagnose_SOURCES = \
        html.h \
        html.c \
        gda-diagnose.c

gda_diagnose_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.0.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
        $(LIBGDA_LIBS)

gda_inspect_dict_file_SOURCES = \
        gda-inspect-dict-file.c

gda_inspect_dict_file_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.0.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
        $(LIBGDA_LIBS)

gda_author_dict_file_SOURCES = \
        gda-author-dict-file.c

gda_author_dict_file_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.0.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
        $(LIBGDA_LIBS)

gda_list_config_SOURCES = \
        gda-list-config.c

gda_list_config_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
        $(top_builddir)/libgda/graph/libgda_graph-3.0.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
        $(LIBGDA_LIBS)

EXTRA_DIST = gda-config-tool.1 gda-config.5
