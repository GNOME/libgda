bin_PROGRAMS = gda-run gda-config-tool

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

