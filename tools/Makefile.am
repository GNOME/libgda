CONFIG_TOOL=

if READLINE_LIB
if HISTORY_LIB
if POPT_LIB
CONFIG_TOOL=gda-config-tool
endif
endif
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

if READLINE_LIB
if HISTORY_LIB
if POPT_LIB
gda_config_tool_SOURCES = gda-config-tool.c
gda_config_tool_LDADD = \
	$(READLINE_LIB) \
	$(HISTORY_LIB) \
	$(POPT_LIB) \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la
endif
endif
endif

