bin_PROGRAMS = \
	gda-list-config-4.0 \
	gda-sql-4.0 \
	gda-list-server-op-4.0

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda \
	-I. \
	$(LIBGDA_CFLAGS)

gda_list_config_4_0_SOURCES = \
        gda-list-config.c

gda_list_config_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

gda_sql_4_0_SOURCES = \
        gda-sql.c \
	tools-input.h \
	tools-input.c \
	command-exec.h \
	command-exec.c

gda_sql_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS) \
	$(READLINE_LIB) \
        $(HISTORY_LIB)

# setenv() is in libiberty in mingw
if PLATFORM_WIN32
gda_sql_4_0_LDADD += -liberty
endif

gda_list_server_op_4_0_SOURCES = \
        gda-list-server-op.c

gda_list_server_op_4_0_LDADD = \
        $(top_builddir)/libgda/libgda-4.0.la \
        $(LIBGDA_LIBS)

