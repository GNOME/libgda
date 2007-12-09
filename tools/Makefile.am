CONFIG_MAN_PAGES = \
	gda-config-tool-3.0.1 \
	gda-config-3.0.5

if BUILD_CONFIG_TOOL
CONFIG_TOOL = gda-config-tool-3.0
man_MANS = $(CONFIG_MAN_PAGES)
else
CONFIG_TOOL=
man_MANS =
endif


bin_PROGRAMS = \
	$(CONFIG_TOOL) \
	gda-diagnose-3.0 \
	gda-list-config-3.0 \
	gda-inspect-dict-file-3.0 \
	gda-author-dict-file-3.0 \
	gda-sql-3.0 \
	gda-list-server-op-3.0

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I. \
	$(LIBGDA_CFLAGS)

gda_config_tool_3_0_SOURCES = gda-config-tool.c
gda_config_tool_3_0_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(READLINE_LIB) \
	$(HISTORY_LIB) \
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

gda_sql_3_0_SOURCES = \
        gda-sql.c \
	tools-input.h \
	tools-input.c \
	command-exec.h \
	command-exec.c

gda_sql_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

# setenv() is in libiberty in mingw
if PLATFORM_WIN32
gda_sql_3_0_LDADD += -liberty
endif

gda_list_server_op_3_0_SOURCES = \
        gda-list-server-op.c

gda_list_server_op_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS)


EXTRA_DIST = $(CONFIG_MAN_PAGES)
