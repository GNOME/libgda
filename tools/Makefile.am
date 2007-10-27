bin_PROGRAMS = \
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

gda_list_server_op_3_0_SOURCES = \
        gda-list-server-op.c

gda_list_server_op_3_0_LDADD = \
        $(top_builddir)/libgda/libgda-3.0.la \
        $(LIBGDA_LIBS)

