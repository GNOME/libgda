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

bin_PROGRAMS = $(RUN_TOOL) $(CONFIG_TOOL)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I. \
	$(LIBGDA_CFLAGS)

gda_run_SOURCES = gda-run.c
gda_run_LDADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(top_builddir)/libgda/graph/libgda_graph-3.la \
        $(top_builddir)/libgda/handlers/libgda_handlers-3.la \
        $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
	$(LIBGDA_LIBS)

gda_config_tool_SOURCES = gda-config-tool.c
gda_config_tool_LDADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(top_builddir)/libgda/graph/libgda_graph-3.la \
	$(top_builddir)/libgda/handlers/libgda_handlers-3.la \
	$(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.la \
	$(READLINE_LIB) \
	$(HISTORY_LIB) \
	$(POPT_LIB) \
	$(LIBGDA_LIBS)

EXTRA_DIST = gda-config-tool.1 gda-config.5
