if BUILD_CONFIG_TOOL
CONFIG_TOOL=gda-config-tool
man_MANS = gda-config-tool.1
EXTRA_DIST = gda-config-tool.1
else
CONFIG_TOOL=
endif

bin_PROGRAMS = gda-run $(CONFIG_TOOL)

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	-I$(top_builddir) \
	-I$(top_srcdir)

gda_run_SOURCES = gda-run.c
gda_run_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la 

gda_config_tool_SOURCES = gda-config-tool.c
gda_config_tool_LDADD = \
	$(READLINE_LIB) \
	$(HISTORY_LIB) \
	$(POPT_LIB) \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la

