if BUILD_CONFIG_TOOL
CONFIG_TOOL=gda-config-tool-3.0
man_MANS = gda-config-tool-3.0.1 \
           gda-config-3.0.5
else
CONFIG_TOOL=
endif

if BUILD_RUN_TOOL
RUN_TOOL=gda-run-3.0
else
RUN_TOOL=
endif

bin_PROGRAMS = \
	$(RUN_TOOL) \
	$(CONFIG_TOOL) \
	gda-diagnose-3.0 \
	gda-list-config-3.0 \
	gda-inspect-dict-file-3.0 \
	gda-author-dict-file-3.0

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I. \
	$(LIBGDA_CFLAGS)

gda_run_3_0_SOURCES = gda-run.c
gda_run_3_0_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

gda_config_tool_3_0_SOURCES = gda-config-tool.c
gda_config_tool_3_0_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(READLINE_LIB) \
	$(HISTORY_LIB) \
	$(POPT_LIB) \
	$(LIBGDA_LIBS)

gda_diagnose_3_0_SOURCES = \
        html.h \
        html.c \
        gda-diagnose.c

gda_diagnose_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS)

gda_inspect_dict_file_3_0_SOURCES = \
        gda-inspect-dict-file.c

gda_inspect_dict_file_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS)

gda_author_dict_file_3_0_SOURCES = \
        gda-author-dict-file.c

gda_author_dict_file_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS)

gda_list_config_3_0_SOURCES = \
        gda-list-config.c

gda_list_config_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS)

EXTRA_DIST = $(man_MANS)
